/*-------------------------------------------------------------------------
This source file is a part of OGRE
(Object-oriented Graphics Rendering Engine)

For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2006 Torus Knot Software Ltd
Also see acknowledgements in Readme.html

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License (LGPL) as 
published by the Free Software Foundation; either version 2.1 of the 
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
License for more details.

You should have received a copy of the GNU Lesser General Public License 
along with this library; if not, write to the Free Software Foundation, 
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA or go to
http://www.gnu.org/copyleft/lesser.txt
-------------------------------------------------------------------------*/
#include "OgreStableHeaders.h"
#include "OgreAllocator.h"
#include "OgreLogManager.h"
#include "OgreAllocator.h"
#include "OgreException.h"

#include "OgreMemoryManager.h"

#include <iostream>
#include <cassert>

// just a handy debugging macro
#define DBG_PROBE(a) std::cout << #a << " : " << a << std::endl;

// OS specific includes
#ifdef WIN32
# include <windows.h>
#elif defined(__GNUC__)
# include <unistd.h>    // sysconf(3)
# include <sys/mman.h>  // mmap(2)
# include <errno.h>
# define USE_MMAP 1
#endif

// allocator 
static Ogre::Allocator<unsigned char>  sAllocator;


// realise static instance member for the init class
Ogre::MemoryManager      Ogre::MemoryManager::smInstance;
Ogre::MemoryManager::Bin Ogre::MemoryManager::mBin[NUM_BINS];
Ogre::MemoryManager::Bin Ogre::MemoryManager::m24ByteBin;


#ifdef WIN32
// win32 POSIX(ish) memory emulation
//--------------------------------------------------------------------

// spin lock
static int gSpinLock;

long getpagesize (void) 
{
    static long gPageSize = 0;
    if (! gPageSize)
    {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        gPageSize = system_info.dwPageSize;
    }
    return gPageSize;
}

long getregionsize (void) 
{
    static long gRegionSize = 0;
    if (! gRegionSize) 
    {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        gRegionSize = system_info.dwAllocationGranularity;
    }
    return gRegionSize;
}

void *mmap (void *ptr, long size, long prot, long type, long handle, long arg) 
{
    static long gPageSize;
    static long gRegionSize;

    // Wait for spin lock
    while (InterlockedCompareExchange ((void **) &gSpinLock, (void *) 1, (void *) 0) != 0) 
        Sleep (0);

    // First time initialisation
    if (! gPageSize) 
        gPageSize = getpagesize ();
    if (! gRegionSize) 
        gRegionSize = getregionsize ();

    // Allocate this
    ptr = VirtualAlloc (ptr, size, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);
    if (! ptr) 
        ptr = MMAP_FAILURE;

    // Release spin lock
    InterlockedExchange (&gSpinLock, 0);
    return ptr;
}

long munmap (void *ptr, long size) 
{
    static long gPageSize;
    static long gRegionSize;
    int rc = MUNMAP_FAILURE;
    // Wait for spin lock
    while (InterlockedCompareExchange ((void **) &gSpinLock, (void *) 1, (void *) 0) != 0) 
        Sleep (0);

    // First time initialisation
    if (! g_pagesize) 
        g_pagesize = getpagesize ();
    if (! g_regionsize) 
        g_regionsize = getregionsize ();

    // Free this
    if ( VirtualFree (ptr, 0, MEM_RELEASE))
        rc = 0;

    // Release spin lock
    InterlockedExchange (&gSpinLock, 0);
    return rc;
}
//--------------------------------------------------------------------
#endif

Ogre::MemoryManager::MemoryManager()
{
    if(!mInited)
        init();
}

Ogre::MemoryManager::~MemoryManager()
{}

void Ogre::MemoryManager::init()
{
    mInited = true;
    mPageSize = getpagesize();
    
 #ifdef __GNUC__
    mVmemStart = mVmemStop = sbrk(0);
    mVmemStart=sbrk((uint32)(mVmemStart)%mPageSize);
    MemProfileManager::getSingleton() << "UNIX/Linux (GNU) Page size is " << mPageSize << " bytes " << mVmemStart << "\n";
#endif
    
    // setup bins
    uint32 sz = (1 << 3);
    for(uint32 i=0;i<NUM_BINS;++i)
    {
        std::cout << "bin " << i << " is " << sz << std::endl;
        mBin[i].setup(i,sz);
        sz <<= 1;
    }
    m24ByteBin.setup(0,24);
}

void* Ogre::MemoryManager::allocMem(size_t size) throw(std::bad_alloc)
{
    if(!mInited)
        init();

#ifdef __GNUC__
    MemBlock memBlock;
    MemCtrl* ret;
    MemCtrl* tmp;

    size += OVERHEAD; // pad up to min aloc size
    size += MIN_ALOC_SIZE - (size % MIN_ALOC_SIZE);

    // find a bin index
    register uint32 idx = 0;      // bins start at 0
    register uint32 shiftVal = 8; // the first bin val = 2^3 = 8

    while (idx < NUM_BINS)
    {
        if (shiftVal >= size)
            break;

        shiftVal <<= 1;
        ++idx;
    }

    // alloc memory from the bin
    if (idx == NUM_BINS)
    {
        MemProfileManager::getSingleton() << "ERROR: asked to alloc a bad size (sz>4G)!";
        throw std::bad_alloc();
        return NULL;
    }

    else
    {
        if (mBin[idx].empty()) // add more memory
        {
            memBlock = moreCore (idx, shiftVal);
        }
        else // alloc from bin
        {
            memBlock.memory = (char*) mBin[idx].pop();
            memBlock.size = shiftVal;
        }

        if (memBlock.size - size == 8)
            size += 8; // just use the entire thing, we cant make any savings

        // build the header block
        ret = (MemCtrl*) memBlock.memory;
        ret->size   = size ^ MASK;
        ret->bin_id = idx;
        ret->magic  = MAGIC;

        // build the tail block
        tmp = (MemCtrl*) (memBlock.memory + (size - sizeof (MemCtrl) ) );
        tmp->size   = size ^ MASK;
        tmp->bin_id = idx;
        tmp->magic  = MAGIC;

        // distribute any remainder
        memBlock.size -= size;

        if (memBlock.size)
        {
            memBlock.memory += size;
            distributeCore (memBlock);
        }

        // return the result trimmed of the header
        return (void*) ( (char*) ret + sizeof (MemCtrl) );
    }

#elif defined(WIN32)
    return malloc(size);
#endif
}

void Ogre::MemoryManager::purgeMem(void* ptr) throw(std::bad_alloc)
{
#ifdef __GNUC__
    assert(ptr < mVmemStop && "BAD POINTER");

    MemBlock memBlock;
    memBlock.memory = (char*)ptr-sizeof(MemCtrl);
    memBlock.size = ((MemCtrl*)memBlock.memory)->size^MASK; // get the size

    // get the head block magic value
    if(((MemCtrl*)memBlock.memory)->magic != MAGIC)
    {
        MemProfileManager::getSingleton() << "Bad pointer passed to purgeMem(), double delete or memory corruption";
        throw std::bad_alloc();
    }

    // get the tail block magic value
    if(((MemCtrl*)(memBlock.memory+(memBlock.size-sizeof(MemCtrl))))->magic != MAGIC)
    {
        MemProfileManager::getSingleton() << "purgeMemory(), Memory corruption detected";
        throw std::bad_alloc();
    }

    // coalesce memory
    //-----------------------------------------------------------------
     /*
    MemFree* memFree;
    register uint32 size;

    while (memBlock.memory + memBlock.size != mVmemStop)
    {
        memFree = (MemFree*) (memBlock.memory + memBlock.size);
        size = memFree->size;

        if (size & MASK) // the block is full, terminate run
        {
            break;
        }
        else // the block is empty, coalesce it
        {
            memBlock.size += size;

            // stitch up the bin
            if(size == 24)
                m24ByteBin.remove(memFree);
            else
            {
                for(uint32 i =0;i < NUM_BINS;++i)
                {
                    if(mBin[i].size() == size)
                    {
                        mBin[i].remove(memFree);
                        break;
                    }
                }
            }
        }
    }
    // */
    //-----------------------------------------------------------------

    distributeCore(memBlock);

    // TODO, restore the wilderness

#elif defined(WIN32)

    free(ptr);
#endif
}

void Ogre::MemoryManager::dumpInternals()
{
    MemBlock memBlock;
    memBlock.memory=(char*)mVmemStart;
    memBlock.size=0;
    
    uint32 size;
    MemFree* memFree;
    MemProfileManager::getSingleton() << "\nDummping Internal Table\n" <<
        "-------------------------------------------------------------\n";
    for (uint32 i = 0;i < NUM_BINS;++i)
        mBin[i].dumpInternals();

    std::cout << "\n24 byte bin " << std::endl;
    m24ByteBin.dumpInternals();

    std::cout << "\nDummping Memory Map\n" <<
        "-------------------------------------------------------------\n";
    while(memBlock.memory + memBlock.size != mVmemStop)
    {
        memFree = (MemFree*)(memBlock.memory + memBlock.size);
        size = memFree->size;

        if(size & MASK)
        {
            MemProfileManager::getSingleton() << "1 : " 
                << (void*)(memBlock.memory + memBlock.size) << " : " << (size^MASK) <<  "\n";
            memBlock.size += (size^MASK);

            MemCtrl* memCtrl = (MemCtrl*)memFree;
            assert(memCtrl->magic == MAGIC);

            memCtrl = (MemCtrl*)((char*)(memFree)+(size^MASK) - sizeof(MemCtrl));
            assert(memCtrl->magic == MAGIC);

        }
        else
        {
            MemProfileManager::getSingleton() << "0 : " 
                << (void*)(memBlock.memory + memBlock.size) << " : " << size << "\n";
            memBlock.size+=size;
        }
        assert(size);
    }
    MemProfileManager::getSingleton() << "-------------------------------------------------------------\n\n";
}

void Ogre::MemoryManager::distributeCore(MemBlock& block)
{
    // we should never have to deal with a block this small
    assert (block.size > 8 && "Block size too small");

    uint32 shiftVal = (1 << NUM_BINS + 2);
    int idx = NUM_BINS - 1;

    // while we have memory to work for, distribute it
    while (idx >= 0 && block.size)
    {
        // There is one problem with allowing this algorithm to
        // naturally terminate. In some cases an 8 byte block is
        // created and needs to be stored, unfortunatly we need
        // at least 12 bytes to hold the header of an un-allocated
        // memory block. So I terminate the algorithm early and
        // allow an out of sequence 24 byte bin to hold the block
        // that would otherwise form one 16 and one 8 byte block
        if (block.size == 24)
        {
            m24ByteBin.push((MemFree*)block.memory);
            block.size = 0;
            break;  // induced termination
        }

        // general case distribution
        if (shiftVal <= block.size)
        {
            if (block.size - shiftVal != 8) // force 24 byte handling
            {
                mBin[idx].push((MemFree*)block.memory);
                block.size -= shiftVal;
                block.memory += shiftVal;
            }
        }
        --idx;
        shiftVal >>= 1;
    }

    // there should never be any left over
    assert (!block.size && "Orphan memory chunck");
}

Ogre::MemoryManager::MemBlock Ogre::MemoryManager::moreCore(uint32 idx, uint32 size)
{
    MemBlock ret;

    register uint32 shiftVal = size;
    register uint32 i = idx;

    // try to find a bigger bin
    while ( mBin[i].empty() && i < NUM_BINS )
    {
        ++i;
        shiftVal <<= 1;
    }
    if ( i < NUM_BINS ) // split a bigger bins memory
    {
        if(shiftVal != mBin[i].size() )
        {
            DBG_PROBE (mBin[i].size());
            DBG_PROBE (shiftVal);
            DBG_PROBE (i);
        }
        assert(mInited);
        assert (shiftVal == mBin[i].size() );

        ret.size = shiftVal;
        ret.memory = (char*) mBin[i].pop();
    }
    else // we need to grab more core
    {
        //std::cout << "---------------------->>>>> more core <<<<<---------------------" << std::endl;

        if (size < mPageSize)
            size = mPageSize;

        ret.size = size;
#ifndef USE_MMAP
        ret.memory= (char*) sbrk( size );
#else
        ret.memory = (char*) mmap(mVmemStop, size, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
#endif
        if (errno==ENOMEM)
        {
            MemProfileManager::getSingleton() << "Out of memory (ENOMEM)";
            throw std::bad_alloc();
        }
        mVmemStop = (void*) ( (char*) mVmemStop + size);
    }
    return ret;
}


int Ogre::MemoryManager::sizeOfStorage(const void* ptr) throw (std::bad_alloc)
{
    if(!ptr)
    {
        MemProfileManager::getSingleton() <<
            "Bad pointer passed to sizeOfStorage() \"NULL\" ";
        throw std::bad_alloc();
    }

    register MemCtrl* head = (MemCtrl*)(((char*)ptr)-sizeof(MemCtrl));
    if(head->magic != MAGIC)
    {

        MemProfileManager::getSingleton() <<
            "Bad pointer passed to sizeOfStorage() or memory corruption" <<
            (void*)head->magic;
        throw std::bad_alloc();
    }
    return head->size^MASK;
}


_OgreExport void *operator new(std::size_t size)
{
    //assert(size && "0 alloc");
    //return Ogre::MemoryManager::getSingleton().allocMem(size);
    return static_cast<void*>(sAllocator.allocateBytes(size));
}

_OgreExport void *operator new[](std::size_t size)
{
    //assert(size && "0 alloc");
    //return Ogre::MemoryManager::getSingleton().allocMem(size);
    return static_cast<void*>(sAllocator.allocateBytes(size));
}

/*
_OgreExport void operator delete(void *ptr, std::size_t size)
{
    //sAllocator.deallocateBytes(static_cast<unsigned char*>(ptr),size);
}

_OgreExport void operator delete[](void *ptr, std::size_t size)
{
    //sAllocator.deallocateBytes(static_cast<unsigned char*>(ptr),size);
}
*/

// /*
_OgreExport void operator delete(void *ptr)
{
    if(ptr==NULL)
        return;
    sAllocator.deallocateBytes(static_cast<unsigned char*>(ptr),0);
    //Ogre::MemoryManager::getSingleton().purgeMem(ptr);
}

_OgreExport void operator delete[](void *ptr)
{
    if(ptr==NULL)
        return;
    sAllocator.deallocateBytes(static_cast<unsigned char*>(ptr),0);
    //Ogre::MemoryManager::getSingleton().purgeMem(ptr);
}
// */

