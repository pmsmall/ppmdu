#include "bmp_io.hpp"
#include <EasyBMP/EasyBMP.h>
#include <ppmdu/utils/library_wide.hpp>
#include <ppmdu/utils/handymath.hpp>
#include <iostream>
#include <algorithm>
using namespace std;

namespace utils{ namespace io
{
    //A little operator to make dealing with finding the color index easier
    inline bool operator==( const RGBApixel & a, const gimg::colorRGB24 & b )
    {
        return (a.Red == b.red) && (a.Green == b.green) && (a.Blue == b.blue);
    }

    //A little operator to make converting color types easier
    inline RGBApixel colorRGB24ToRGBApixel( const gimg::colorRGB24 & c )
    {
        RGBApixel tmp;
        tmp.Red   = c.red;
        tmp.Green = c.green;
        tmp.Blue  = c.blue;
        return tmp;
    }

    //A little operator to make converting color types easier
    inline gimg::colorRGB24 RGBApixelTocolorRGB24( const RGBApixel & c )
    {
        gimg::colorRGB24 tmp;
        tmp.red   = c.Red;
        tmp.green = c.Green;
        tmp.blue  = c.Blue;
        return tmp;
    }

    //Returns -1 if it doesn't find the color !
    template< class paletteT >
        inline int FindIndexForColor( const RGBApixel & pix, const paletteT & palette )
    {
        for( unsigned int i = 0; i < palette.size(); ++i )
        {
            if( pix == palette[i] )
                return i;
        }

        return -1;
    }

    bool ImportFrom4bppBMP( gimg::tiled_image_i4bpp & out_indexed,
                            const std::string       & filepath,
                            unsigned int              forcedwidth,
                            unsigned int              forcedheight,
                            bool                      erroronwrongres)
    {
        bool   hasWarnedOORPixel = false; //Whether we warned about out of range pixels at least once during the pixel loop! If applicable..
        bool   isWrongResolution = false;
        BMP    input;
        auto & outpal = out_indexed.getPalette();

        input.ReadFromFile( filepath.c_str() );

        if( input.TellBitDepth() != 4 )
        {
            //We don't support anything not 4 bpp !
            //Mention the palette length mismatch
            cerr <<"\n<!>-ERROR: The file : " <<filepath <<", is not a 4bpp indexed bmp ! Its in fact " <<input.TellBitDepth() <<"bpp!\n"
                 <<"Next time make sure the bmp file is saved as 4bpp, 16 colors!\n";

            return false;
        }

        //Only force resolution when we were asked to!
        if( forcedwidth != 0 && forcedheight != 0 )
            isWrongResolution = input.TellWidth() != forcedwidth || input.TellHeight() != forcedheight;
        if( erroronwrongres && isWrongResolution )
        {
            cerr <<"\n<!>-ERROR: The file : " <<filepath <<" has an unexpected resolution!\n"
                 <<"             Expected :" <<forcedwidth <<"x" <<forcedheight <<", and got " <<input.TellWidth() <<"x" <<input.TellHeight() 
                 <<"! Skipping!\n";
            return false;
        }

        if( utils::LibraryWide::getInstance().Data().isVerboseOn() && input.TellNumberOfColors() <= 16 )
        {
            //Mention the palette length mismatch
            cerr <<"\n<!>-Warning: " <<filepath <<" has a different palette length than expected!\n"
                 <<"Fixing and continuing happily..\n";
        }
        out_indexed.setNbColors( 16u );

        //Build palette
        for( int i = 0; i < input.TellNumberOfColors(); ++i )
        {
            RGBApixel acolor = input.GetColor(i);
            outpal[i].red   = acolor.Red;
            outpal[i].green = acolor.Green;
            outpal[i].blue  = acolor.Blue;
            //Alpha is ignored
        }

        //Image Resolution
        int tiledwidth  = (forcedwidth != 0)?  forcedwidth  : input.TellWidth();
        int tiledheight = (forcedheight != 0)? forcedheight : input.TellHeight();

        //Make sure the height and width are divisible by the size of the tiles!
        if( tiledwidth % gimg::tiled_image_i4bpp::tile_t::WIDTH )
            tiledwidth = CalcClosestHighestDenominator( tiledwidth,  gimg::tiled_image_i4bpp::tile_t::WIDTH );

        if( tiledheight % gimg::tiled_image_i4bpp::tile_t::HEIGHT )
            tiledheight = CalcClosestHighestDenominator( tiledheight,  gimg::tiled_image_i4bpp::tile_t::HEIGHT );

        //Resize target image
        out_indexed.setPixelResolution( tiledwidth, tiledheight );

        //If the image we read is not divisible by the dimension of our tiles, 
        // we have to ensure that we won't got out of bound while copying!
        int maxCopyWidth  = out_indexed.getNbPixelWidth();
        int maxCopyHeight = out_indexed.getNbPixelHeight();

        if( maxCopyWidth != input.TellWidth() || maxCopyHeight != input.TellHeight() )
        {
            //Take the smallest resolution, so we don't go out of bound!
            maxCopyWidth  = std::min( input.TellWidth(),  maxCopyWidth );
            maxCopyHeight = std::min( input.TellHeight(), maxCopyHeight );
        }

        //Copy pixels over
        for( int i = 0; i < maxCopyWidth; ++i )
        {
            for( int j = 0; j < maxCopyHeight; ++j )
            {
                RGBApixel apixel = input.GetPixel(i,j);

                //First we need to find out what index the color is..
                gimg::colorRGB24::colordata_t colorindex = FindIndexForColor( apixel, outpal );

                if( !hasWarnedOORPixel && colorindex == -1 )
                {
                    //We got a problem
                    cerr <<"\n<!>-Warning: Image " <<filepath <<", has pixels with colors that aren't in the colormap/palette!\n"
                         <<"Defaulting pixels out of range to color 0!\n";
                    hasWarnedOORPixel = true;
                    colorindex        = 0;
                }

                out_indexed.getPixel( i, j ) = colorindex;
            }
        }

        return true;
    }



    //bool ImportFrom4bppBMP( gimg::tiled_image_i4bpp & out_indexed,
    //                        const std::string       & filepath,
    //                        unsigned int              forcedwidth,
    //                        unsigned int              forcedheight,
    //                        bool                      erroronwrongres )
    //{
    //    bool   hasWarnedOORPixel = false; //Whether we warned about out of range pixels at least once during the pixel loop! If applicable..
    //    bool   isWrongResolution = false;
    //    BMP    input;
    //    auto & outpal = out_indexed.getPalette();


    //    input.ReadFromFile( filepath.c_str() );

    //    if( input.TellBitDepth() != 4 )
    //    {
    //        //We don't support anything not 4 bpp !
    //        //Mention the palette length mismatch
    //        cerr <<"\n<!>-ERROR: The file : " <<filepath <<", is not a 4bpp indexed bmp ! Its in fact " <<input.TellBitDepth() <<"bpp!\n"
    //             <<"Next time make sure the bmp file is saved as 4bpp, 16 colors!\n";

    //        return false;
    //    }


    //    isWrongResolution = input.TellWidth() != forcedwidth || input.TellHeight() != forcedheight;

    //    if( erroronwrongres && isWrongResolution )
    //    {
    //        cerr <<"\n<!>-ERROR: The file : " <<filepath <<" has an unexpected resolution!\n"
    //             <<"             Expected :" <<forcedwidth <<"x" <<forcedheight <<", and got " <<input.TellWidth() <<"x" <<input.TellHeight() 
    //             <<"! Skipping!\n";
    //        return false;
    //    }

    //    if( utils::LibraryWide::getInstance().Data().isVerboseOn() && input.TellNumberOfColors() <= 16 )
    //    {
    //        //Mention the palette length mismatch
    //        cerr <<"\n<!>-Warning: " <<filepath <<" has a different palette length than expected!\n"
    //             <<"Fixing and continuing happily..\n";
    //    }
    //    out_indexed.setNbColors( 16u );

    //    //Build palette
    //    for( int i = 0; i < input.TellNumberOfColors(); ++i )
    //    {
    //        RGBApixel acolor = input.GetColor(i);
    //        outpal[i].red   = acolor.Red;
    //        outpal[i].green = acolor.Green;
    //        outpal[i].blue  = acolor.Blue;
    //        //Alpha is ignored
    //    }

    //    //Resize target image
    //    out_indexed.setPixelResolution( forcedwidth, forcedheight );

    //    int maxCopyWidth  = input.TellWidth();
    //    int maxCopyHeight = input.TellHeight();

    //    if( isWrongResolution )
    //    {
    //        //Take the smallest resolution, so we don't go out of bound!
    //        maxCopyWidth  = std::min( static_cast<int>(out_indexed.getNbPixelWidth()),  maxCopyWidth );
    //        maxCopyHeight = std::min( static_cast<int>(out_indexed.getNbPixelHeight()), maxCopyHeight );
    //    }

    //    //Copy pixels over
    //    for( int i = 0; i < maxCopyWidth; ++i )
    //    {
    //        for( int j = 0; j < maxCopyHeight; ++j )
    //        {
    //            RGBApixel apixel = input.GetPixel(i,j);

    //            //First we need to find out what index the color is..
    //            gimg::colorRGB24::colordata_t colorindex = FindIndexForColor( apixel, outpal );

    //            if( !hasWarnedOORPixel && colorindex == -1 )
    //            {
    //                //We got a problem
    //                cerr <<"\n<!>-Warning: Image " <<filepath <<", has pixels with colors that aren't in the colormap/palette!\n"
    //                     <<"Defaulting pixels out of range to color 0!\n";
    //                hasWarnedOORPixel = true;
    //                colorindex        = 0;
    //            }

    //            out_indexed.getPixel( i, j ) = colorindex;
    //        }
    //    }

    //    return true;
    //}


    bool ExportTo4bppBMP( const gimg::tiled_image_i4bpp & in_indexed,
                          const std::string             & filepath )
    {
        BMP output;
        output.SetBitDepth(4);
        assert( in_indexed.getNbColors() == 16 );

        //copy palette
        for( int i = 0; i < output.TellNumberOfColors(); ++i )
            output.SetColor( i, colorRGB24ToRGBApixel( in_indexed.getPalette()[i] ) );

        //Copy image
        output.SetSize( in_indexed.getNbPixelWidth(), in_indexed.getNbPixelHeight() );

        for( int i = 0; i < output.TellWidth(); ++i )
        {
            for( int j = 0; j < output.TellHeight(); ++j )
            {
                auto &  refpixel = in_indexed.getPixel( i, j );
                uint8_t temp     = static_cast<uint8_t>( refpixel.getWholePixelData() );
                output.SetPixel( i,j, colorRGB24ToRGBApixel( in_indexed.getPalette()[temp] ) ); //We need to input the color directly thnaks to EasyBMP
            }
        }

        bool bsuccess = false;
        try
        {
            bsuccess = output.WriteToFile( filepath.c_str() );
        }
        catch( std::exception e )
        {
            cerr << "<!>- Error outputing image : " << filepath <<"\n"
                 << "     Exception details : \n"     
                 << "        " <<e.what()  <<"\n";

            assert(false);
            bsuccess = false;
        }
        return bsuccess;
    }
};};