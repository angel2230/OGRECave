/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2005 The OGRE Team
Also see acknowledgements in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/
#include "Ogre.h"
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace Ogre;

class PixelFormatTests : public CppUnit::TestFixture
{
    // CppUnit macros for setting up the test suite
    CPPUNIT_TEST_SUITE( PixelFormatTests );
    CPPUNIT_TEST( testIntegerPackUnpack );
    CPPUNIT_TEST( testFloatPackUnpack );    
    CPPUNIT_TEST( testBulkConversion );
    CPPUNIT_TEST_SUITE_END();
public:
    void setUp();
    void tearDown();
    
    void testIntegerPackUnpack();
    void testFloatPackUnpack();    
    void testBulkConversion();

    // Utils
    void setupBoxes(PixelFormat srcFormat, PixelFormat dstFormat);
    void testCase(PixelFormat srcFormat, PixelFormat dstFormat);    
private:
    int size;
    uint8 *randomData;
    uint8 *temp, *temp2;
    PixelBox src, dst1, dst2;
};
