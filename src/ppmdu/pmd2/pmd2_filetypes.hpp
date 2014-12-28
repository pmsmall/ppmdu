#ifndef PMD2_FILETYPES_HPP
#define PMD2_FILETYPES_HPP
/*
pmd2_filetypes.hpp
24/05/2014
psycommando@gmail.com

Description:


No crappyrights. All wrong reversed !
*/
#include <string>
#include <vector>
#include <ppmdu/basetypes.hpp>

namespace pmd2 { namespace filetypes 
{
//==================================================================
// Typedefs
//==================================================================
    typedef unsigned int filesize_t;

//==================================================================
// Constants
//==================================================================
    namespace magicnumbers
    {
	    //Common Magic Numbers 
	    //static const uint32_t     SIR0_MAGIC_NUMBER_LENGTH					  = 4;
	    //static const uint8_t      SIR0_MAGIC_NUMBER[SIR0_MAGIC_NUMBER_LENGTH] = { 0x53, 0x49, 0x52, 0x30 }; //"SIR0"
        static const std::array<uint8_t,4> SIR0_MAGIC_NUMBER = { 0x53, 0x49, 0x52, 0x30 }; //"SIR0"
        static const uint32_t     SIR0_MAGIC_NUMBER_INT                       = 0x53495230; //"SIR0", stored as unsigned int for convenience

	    //static const uint32_t     PKDPX_MAGIC_NUMBER_LENGTH					   = 5;
	    //static const uint8_t      PKDPX_MAGIC_NUMBER[PKDPX_MAGIC_NUMBER_LENGTH]= { 0x50, 0x4B, 0x44, 0x50, 0x58 };
        static const std::array<uint8_t,5> PKDPX_MAGIC_NUMBER = { 0x50, 0x4B, 0x44, 0x50, 0x58 }; //"PKDPX"

	    //static const uint32_t     AT4PX_MAGIC_NUMBER_LENGTH					   = 5;
	    //static const uint8_t      AT4PX_MAGIC_NUMBER[AT4PX_MAGIC_NUMBER_LENGTH]= { 0x41, 0x54, 0x34, 0x50, 0x58 };
        static const std::array<uint8_t,5> AT4PX_MAGIC_NUMBER = { 0x41, 0x54, 0x34, 0x50, 0x58 }; //"AT4PX"
    };

    //File extensions used in the library
    static const std::string AT4PX_FILEX             = "at4px";
    static const std::string IMAGE_RAW_FILEX         = "rawimg";
    static const std::string KAOMADO_FILEX           = "kao";
    static const std::string PACK_FILEX              = "bin";
    static const std::string PALETTE_RAW_RGB24_FILEX = "rawrgb24pal";
    static const std::string PKDPX_FILEX             = "pkdpx";
    static const std::string SIR0_FILEX              = "sir0";
    static const std::string WAN_FILEX               = "wan";
    

    //

    //Padding bytes
    static const uint8_t COMMON_PADDING_BYTE        = 0xAA; //The most common padding byte in all PMD2 files !
    static const uint8_t PF_PADDING_BYTE            = 0xFF; // Padding character used by the Pack file for padding files and the header

//==================================================================
// Enums
//==================================================================
    /*
        The possible content types that can be found within a file / another container.
    */
    enum struct e_ContentType : unsigned int
    {
        PKDPX_CONTAINER,
        AT4PX_CONTAINER,
        SIR0_CONTAINER,
        PALETTE_15_BITS_3_Bytes_16_COLORS,  //#TODO: Is it even used anymore ? remove if unused !!
        SPRITE_CONTAINER,
        COMPRESSED_DATA,    //For the content of PKDPX container, given we can't decompress when analysing
        PACK_CONTAINER,
        KAOMADO_CONTAINER,

        //#If you add any more, be sure to modify GetContentTypeName
        UNKNOWN_CONTENT,
    };

//==================================================================
// Functions
//==================================================================
    //For the given magic number, the function returns a file extension that correspond to the filetype ! 
    //std::string GetAppropriateFileExtension( const std::vector<uint8_t> & filedata );

    std::string GetAppropriateFileExtension( std::vector<uint8_t>::const_iterator & itdatabeg,
                                             std::vector<uint8_t>::const_iterator & itdataend );

    //Returns a short string identifying what is the type of content is in this kind of file !
    std::string GetContentTypeName( e_ContentType type );

};};

#endif