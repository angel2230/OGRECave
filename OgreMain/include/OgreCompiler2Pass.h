/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.stevestreeting.com/ogre/

Copyright (c) 2000-2005 The OGRE Team
Also see acknowledgements in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/gpl.html.
-----------------------------------------------------------------------------
*/


#ifndef __Compiler2Pass_H__
#define __Compiler2Pass_H__

#include <vector>
#include "OgrePrerequisites.h"

namespace Ogre {

    // macro to get the size of a static array
    #define ARRAYSIZE(array) (sizeof(array)/sizeof(array[0]))

    typedef bool (*TOKEN_ACTION)(void);


    /** Compiler2Pass is a generic 2 pass compiler/assembler
    @remarks
	    provides a tokenizer in pass 1 and relies on the subclass to provide the virtual method for pass 2 

	    PASS 1 - tokenize source: this is a simple brute force lexical scanner/analyzer that also parses
			    the formed token for proper semantics and context in one pass
			    it uses Look Ahead Left-Right (LALR) ruling based on Backus - Naur Form (BNF) notation for semantic
			    checking and also performs	context checking allowing for language dialects

	    PASS 2 - generate application specific instructions ie native instructions based on the tokens in the instruction container.

    @par
	    this class must be subclassed with the subclass providing some implementation details for Pass 2.  The subclass
	    is responsible for setting up the token libraries along with defining the language syntax and handling
        token actions during the second pass.

    @par
        The sub class normally supplies a simplified BNF text description in its constructor prior to doing any parsing/tokenizing of source.
        The simplified BNF text description defines the language syntax and rule structure.
        The meta-symbols used in the BNF text description are: 

        ::=  meaning "is defined as". "::=" starts the definition of a rule.  The left side of ::= must contain an <identifier>

        <>   angle brackets are used to surround syntax rule names. A syntax rule name is also called a non-terminal in that
             it does not generate a terminal token in the instruction container for pass 2 processing.

        |    meaning "or". if the item on the left of the | fails then the item on the right is tested.
             Example: <true_false> ::= 'true' | 'false';
             whitespace is used to imply AND operation between left and right items.
             Example: <terrain_shadaws> ::= 'terrain_shadows' <true_false>
             the 'terrain_shadows' terminal token must be found and <true_false> rule must pass in order for <terrain_shadows> rule
             to pass.


        []   optional rule identifier is enclosed in meta symbols [ and ].
             Note that only one identifier or terminal token can take [] modifier.
        {}   repetitive identifier (zero or more times) is enclosed in meta symbols { and }
             Note that only one identifier or terminal token can take {} modifier.
        ''   terminal tokens are surrounded by single quotes.  A terminal token is always one or more characters.
             For example: 'Colour' defines a character sequence that must be matched in whole.  Note that matching is case
             sensitive.
        -''  no terminal token is generated when a - precedes the first single quote but the text in between the quotes is still
             tested against the characters in the source being parsed.
        <#name> # indicates that a numerical value is to be parsed to form a terminal token.  Name is optional and is just a descriptor 
             to help with understanding what the value will be used for.
             that will hold the value.
             Example: <Colour> ::= <#red> <#green> <#blue>
        ()   parentheses enclose a set of characters that can be used to generate a user identifier. for example:
             (0123456789) matches a single character found in that set.
             An example of a user identifier:

             <Label> ::= <Character> {<Character>};
             <Character> ::= (abcdefghijklmnopqrstuvwxyz);

             This will generate a rule that accepts one or more lowercase letters to make up the Label.  The User identifier
             stops collecting the characters into a string when a match cannot be found in the rule.

         ;   marks end of rule

    */
    class _OgreExport Compiler2Pass
    {

    protected:

	    // BNF operation types
	    enum OperationType {otRULE, otAND, otOR, otOPTIONAL, otREPEAT, otEND, otEND_EXECUTE};

	    /** structure used to build rule paths

	    */
	    struct TokenRule
        {
		    OperationType mOperation;
		    uint mTokenID;
		    char* mSymbol;
		    uint mErrorID;
	    };

        enum SystemRuleToken {
            _no_token_ = 1000,
            _character_,
            _value_
        };

	    enum BNF_ID {
            BNF_SYNTAX, BNF_RULE, BNF_IDENTIFIER, BNF_SET_RULE, BNF_EXPRESSION,
            BNF_REPEAT_EXPRESSION, BNF_REPEAT, BNF_OPTIONAL, BNF_OPTIONAL_EXPRESSION,

            BNF_LETTER, BNF_LETTER_DIGIT, BNF_DIGIT
        };

        //  mainly used by bootstrap BNF text parser
        //  child class of Copmiler2Pass could use it to statically define rules
        // <>	- non-terminal token
        #define _rule_		{otRULE,		        // ::=	- rule definition
        #define _is_		,0},{otAND,
        #define _and_		_is_    		        //      - blank space is an implied "AND" meaning the token is required
        #define _or_		,0},{otOR,		        // |	- or
        #define _optional_	,0},{otOPTIONAL,	    // []	- optional
        #define _repeat_	,0},{otREPEAT,	        // {}	- repeat 0 or more times until fail or rule does not progress
        #define _end_		,0},{otEND,0,0,0},
        #define _execute_	,0},{otEND_EXECUTE,0,0,0},
        #define _nt_        ,0
        // " "  - terminal token string

	    /** structure used to build Symbol Type library */
	    struct SymbolDef
        {
	    size_t mID;					/// Token ID which is the index into the Token Type library
	    uint mPass2Data;			/// data used by pass 2 to build native instructions

	    uint mContextKey;			/// context key to fit the Active Context 
	    uint mContextPatternSet;	/// new pattern to set for Active Context bits
	    uint mContextPatternClear;  /// Contexts bits to clear Active Context bits

	    size_t mDefTextID;			/// index into text table for default name : set at runtime
	    size_t mRuleID;				/// index into Rule database for non-terminal token rulepath
								    /// if RuleID is zero the token is terminal

	    };

        struct TokenDef
        {
            size_t ID;
            bool hasAction;

            TokenDef(void) : ID(0), hasAction(false) {}
            TokenDef( const size_t _ID, const bool _hasAction )
                : ID(_ID)
                , hasAction(_hasAction)
            {
            }
        };

        typedef std::map<std::string, TokenDef> LexemeTokenMap;
        typedef LexemeTokenMap::iterator TokeyKeyIterator;
        LexemeTokenMap mLexemeTokenMap;

	    /** structure for Token instructions that are constructed during first pass*/
	    struct TokenInst
        {
	    size_t mNTTRuleID;			/// Non-Terminal Token Rule ID that generated Token
	    size_t mID;					/// expected Token ID. Could be UNKNOWN if valid token was not found.
	    size_t mLine;				/// line number in source code where Token was found
	    size_t mPos;				/// Character position in source where Token was found
        bool mFound;                /// is true if expected token was found
	    };

	    typedef std::vector<TokenInst> TokenInstContainer;
	    //typedef TokenInstContainer::iterator TokenInstIterator;

	    /// container for Tokens extracted from source
	    TokenInstContainer mTokenInstructions;
        /// the location within the token instruction container where pass 2 is
        size_t mPass2TokenPosition;

	    /// pointer to the source to be compiled
	    const char* mSource;
	    size_t mEndOfSource;

	    /// pointers to Text and Token Type libraries setup by subclass
	    SymbolDef* mSymbolTypeLib;

	    /// pointer to root rule path - has to be set by subclass constructor
	    TokenRule* mRootRulePath;

	    /// number of entries in Text and Token Type libraries
	    size_t mRulePathLibCnt;
	    size_t mSymbolTypeLibCnt;

	    // mVauleID, mLabelID, mBadTokenID need to be initialized by the subclass before compiling occurs
	    // defines the token ID used in the symbol type library
	    //uint mValueID;
        //uint mLabelID;
        //uint mBadTokenID;

	    size_t mCurrentLine; /// current line number in source being tokenized
	    size_t mCharPos;     /// position in current line in source being tokenized

	    /// storage container for constants defined in source
        /// container uses Token index as a key associated with a float constant
	    std::map<size_t, float> mConstants;
	    /// storage container for string labels defined in source
        /// container uses Token index as a key associated with a label
        std::map<size_t, String> mLabels;
        /// flag indicates when a label is being parsed.
        /// It gets set false when a terminal token not of _character_ is encountered
        bool mLabelIsActive;
        /// the key of the active label being built during pass 1.
        /// a new key is calculated when mLabelIsActive switches from false to true
        size_t mActiveLabelKey;

	    /// Active Contexts pattern used in pass 1 to determine which tokens are valid for a certain context
	    uint mActiveContexts;

        /// BNF rules to parse BNF text form
	    static TokenRule BNF_RulePath[];

	    /** check token semantics between ID1 and ID2 using left/right semantic data in Token Type library
	    @param ID1 token ID on the left 
	    @param ID2 token ID on the right
	    @return
		    true if both will bind to each other
		    false if either fails the semantic bind test

	    */
	    //bool checkTokenSemantics(uint ID1, uint ID2);

	    /** perform pass 1 of compile process
		    scans source for symbols that can be tokenized and then
		    performs general semantic and context verification on each symbol before it is tokenized.
		    A tokenized instruction list is built to be used by Pass 2.
            A rule path can trigger Pass 2 execution if enough tokens have been generated in Pass 1.
            Pass 1 will then pass control to pass 2 temporarily until the current tokens have been consumed.

	    */
	    bool doPass1();

	    /** performs Pass 2 of compile process which is execution of the tokens
	    @remark
		    Pass 2 takes the token instructions generated in Pass 1 and
		    builds the application specific instructions along with verifying
		    symantic and context rules that could not be checked in Pass 1.
        @par
            Pass 2 execution consumes tokens and moves the Pass 2 token instruction position towards the end
            of the token container.  Token execution can insert new tokens into the token container.
	    */
	    bool doPass2();

        /** execute the action associated with the token pointed to by the Pass 2 token instruction position.
            Its upto the child class to implement how it will associate a token key with and action.
        */
        virtual void executeTokenAction(const size_t tokenID) = 0;
        /** Gets the next token from the instruction que.  If an unkown token is found then an exception is raised but
            the instruction pointer is still moved passed the unknown token.  The subclass should catch the exception,
            provide an error message, and attempt recovery.
        */
        const TokenInst& getNextToken(void);
        /** 
        */
        void replaceToken(void);
        /** Gets the next token's associated floating point value in the instruction que that was parsed from the
            text source.  If an unkown token is found or no associated value was found then an exception is raised but
            the instruction pointer is still moved passed the unknown token.  The subclass should catch the exception,
            provide an error message, and attempt recovery.
        */
        float getNextTokenValue(void);
        /** Gets the next token's associated text label in the instruction que that was parsed from the
            text source.  If an unkown token is found or no associated label was found then an exception is raised but
            the instruction pointer is still moved passed the unknown token.  The subclass should catch the exception,
            provide an error message, and attempt recovery.
        */
        const String& getNextTokenLabel(void);
        /** Gets the number of tokens waiting in the instruction que that need to be processed by an token action.
        */
        size_t getTokenQueCount(void);

        /** Add a lexeme token association.  The backend compiler uses the associations between lexeme
            and token when building the rule base from the BNF script so all associations must be done
            prior to compiling a source.
        */
        void addLexemeToken(const std::string& lexeme, const size_t token, const bool hasAction = false);

        /// find the eol charater
	    void findEOL();

	    /** get the text symbol for this token
	    @remark
		    mainly used for debugging and in test routines
	    @param sid is the token ID
	    @return a pointer to the string text
	    */
	    char* getTypeDefText(const uint sid);

	    /** check to see if the text at the present position in the source is a numerical constant
	    @param fvalue is a reference that will receive the float value that is in the source
	    @param charsize reference to receive number of characters that make of the value in the source
	    @return
		    true if characters form a valid float representation
		    false if a number value could not be extracted
	    */
	    bool isFloatValue(float& fvalue, size_t& charsize);

	    /** check to see if the text is in the symbol text library
	    @param symbol points to begining of text where a symbol token might exist
	    @param symbolsize reference that will receive the size value of the symbol found
	    @return
		    true if a matching token could be found in the token type library
		    false if could not be tokenized
	    */
	    bool isSymbol(const char* symbol, size_t& symbolsize);
	    /// position to the next possible valid sysmbol
	    bool positionToNextSymbol();
	    /** process input source text using rulepath to determine allowed tokens
	    @remarks
		    the method is reentrant and recursive
		    if a non-terminal token is encountered in the current rule path then the method is
		    called using the new rule path referenced by the non-terminal token
		    Tokens can have the following operation states which effects the flow path of the rule
			    RULE: defines a rule path for the non-terminal token
			    AND: the token is required for the rule to pass
			    OR: if the previous tokens failed then try these ones
			    OPTIONAL: the token is optional and does not cause the rule to fail if the token is not found
			    REPEAT: the token is required but there can be more than one in a sequence
			    END: end of the rule path - the method returns the succuss of the rule
                EXECUTE: same as END but instead of continuing Pass 1 tokenizing, Pass 2 is initiated to consume
                  current tokens.

	    @param rulepathIDX index into an array of Token Rules that define a rule path to be processed
	    @return 
		    true if rule passed - all required tokens found
		    false if one or more tokens required to complete the rule were not found
	    */
	    bool processRulePath( size_t rulepathIDX);

    	
	    /** setup ActiveContexts - should be called by subclass to setup initial language contexts
        */
	    void setActiveContexts(const uint contexts){ mActiveContexts = contexts; }

	    /// comment specifiers are hard coded
	    void skipComments();

	    /// find end of line marker and move past it
	    void skipEOL();

	    /// skip all the white space which includes spaces and tabs
	    void skipWhiteSpace();


	    /** check if current position in source has the symbol text equivalent to the TokenID
	    @param rulepathIDX index into rule path database of token to validate
	    @param activeRuleID index of non-terminal rule that generated the token
	    @return
		    true if token was found
		    false if token symbol text does not match the source text
		    if token is non-terminal then processRulePath is called 
	    */
	    bool ValidateToken(const size_t rulepathIDX, const uint activeRuleID); 


    public:


	    /// constructor
	    Compiler2Pass();
        virtual ~Compiler2Pass() {}

	    /** compile the source - performs 2 passes.
		    First pass is to tokinize, check semantics and context.
		    The second pass is performed by using tokens to look up function implementors and executing
            them which convert tokens to application specific instructions.
	    @remark
		    Pass 2 only gets executed if Pass 1 has built enough tokens to complete a rule path and found no errors
	    @param source a pointer to the source text to be compiled
	    @return
		    true if Pass 1 and Pass 2 are successfull
		    false if any errors occur in Pass 1 or Pass 2
	    */
	    bool compile(const char* source);

	    /** Initialize the type library with matching symbol text found in symbol text library
		    find a default text for all Symbol Types in library

		    scan through all the rules and initialize TypeLib with index to text and index to rules for non-terminal tokens

		    must be called by subclass after libraries and rule database setup
	    */

	    void InitSymbolTypeLib();

    };

}

#endif
