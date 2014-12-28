#ifndef TILED_IMAGE_HPP
#define TILED_IMAGE_HPP
/*
tiled_image.hpp
2014/11/18
psycommando@gmail.com
Description: A set of utilities for dealing with and storing tiled images for
             quicker conversion, parsing, and writing of tiled images.
*/
#include <numeric>
#include <cstdint>
#include <array>
#include <vector>
#include <bitset>
#include <cassert>
#include <cmath>
#include <sstream>
#include "color.hpp"
#include <ppmdu/utils/utility.hpp>
//#include <ppmdu/pmd2/pmd2_image_formats.hpp>
#include "index_iterator.hpp"
#include "img_pixel.hpp"
#include "base_image.hpp"

//#TODO: Should we remove the pmd2 namespace ?
namespace gimg
{
//=============================================================================
// Tile
//=============================================================================
    /*************************************************************************************************
        tile
        A class representing a single tile within a tiled image.
        Tiles by default are 8x8, but can be modified using the template parameters.

        This class doesn't require a specific kind of _PIXEL_T, since it act only as 
        a container.

        The tile size is static, even though the internal container is a dynamically
        allocated std::vector. This is simply because moving std::vector is possible,
        while moving std::array is not. This means you can only copy std::array,
        and this implies a linear complexity depending on the size of the array..
        While a Move on a std::vector is a constant complexity..

        Using the move constructor or move assignement operator allows significantly 
        better performances!
    *************************************************************************************************/
    template<class _PIXEL_T, unsigned int _tilewidth = 8u, unsigned int _tileheight = 8u> //#TODO: should remove the "_tilewidth" and "_tileheight" params, and make them default constructor params instead. It would make things clearer.
        class tile
    {
    public:
        typedef _PIXEL_T pixel_t;
        static const unsigned int WIDTH     = _tilewidth;
        static const unsigned int HEIGHT    = _tileheight;
        static const unsigned int NB_PIXELS = WIDTH * HEIGHT;

        tile()
            :content( HEIGHT, std::vector<pixel_t>(WIDTH) )
        {
        }

        //Copy contructor
        tile( const tile<_PIXEL_T, _tilewidth, _tileheight>  & other )
        {
            content = other.content;
        }

        //Copy assignement operator
        const tile<_PIXEL_T, _tilewidth, _tileheight> & operator=( const tile<_PIXEL_T, _tilewidth, _tileheight>  & other )
        {
            content = other.content;
            return *this;
        }

        //Move constructor
        tile( tile<_PIXEL_T, _tilewidth, _tileheight>  && other )
        {
            content = std::move_if_noexcept(other.content);
        }

        //Move assignement operator
        inline const tile<_PIXEL_T, _tilewidth, _tileheight> & operator=( tile<_PIXEL_T, _tilewidth, _tileheight>  && other )
        {
            content = std::move_if_noexcept(other.content);
            return *this;
        }

        /*
            operator[]
        */
        inline pixel_t       & operator[]( unsigned int pos )      { return content[pos / WIDTH][pos % WIDTH]; }
        inline const pixel_t & operator[]( unsigned int pos )const { return content[pos / WIDTH][pos % WIDTH]; }
        /*
            getPixel
        */
        inline pixel_t       & getPixel( unsigned int x, unsigned int y )      { return content[y][x]; }
        inline const pixel_t & getPixel( unsigned int x, unsigned int y )const { return content[y][x]; }

    private:
        std::vector<std::vector<pixel_t> > content;
    };

//=============================================================================
// Tiled Image
//=============================================================================

    /*************************************************************************************************
        tiled_image
            An image class for containing data from a tiled image.

            It allows seamlessly to handle the data as if it was either stored in a linear
            vector, or as a 2D image, where each pixels can be accessed via X,Y coordinates.
            It also allow to operate directly on individual tiles!

            Its size can be dynamically changed, however, the size of the individual tiles 
            is static.
    *************************************************************************************************/
    template< class _PIXEL_T, unsigned int _TILE_NB_ROWS = 8, unsigned int _TILE_NB_COLS = 8 >
        class tiled_image : public base_image< 
                                                tiled_image<_PIXEL_T, 
                                                            _TILE_NB_ROWS, 
                                                            _TILE_NB_COLS>, 
                                                _PIXEL_T>
    {
    public:
        typedef tile<_PIXEL_T,_TILE_NB_ROWS,_TILE_NB_COLS>      tile_t;
        typedef typename tile_t::pixel_t                        pixel_t;
        typedef typename tile_t::pixel_t                        value_type; //For the iterator
        typedef utils::index_iterator<tiled_image>              iterator;
        typedef utils::const_index_iterator<const tiled_image>  const_iterator;

        //------ Construction ------
        tiled_image() throw()
            :m_totalNbPixels(0), m_pixelWidth(0), m_pixelHeight(0),m_nbTileRows(0), m_nbTileColumns(0)
        {
        }

        tiled_image( unsigned int pixelsWidth, unsigned int pixelsHeigth ) throw()
            :m_totalNbPixels(0), m_pixelWidth(0), m_pixelHeight(0),m_nbTileRows(0), m_nbTileColumns(0)
        {
            setPixelResolution(pixelsWidth,pixelsHeigth);
        }

        //Copy
        tiled_image( const tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> & other ) throw()
            :m_totalNbPixels(0), m_pixelWidth(0), m_pixelHeight(0),m_nbTileRows(0), m_nbTileColumns(0)
        {
            copyFrom(&other);
        }

        //Move
        tiled_image( tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> && other ) throw()
            :m_totalNbPixels(0), m_pixelWidth(0), m_pixelHeight(0),m_nbTileRows(0), m_nbTileColumns(0)
        {
            moveFrom(&other);
        }

        //Copy
        const tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> & operator=( const tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> & other ) throw()
        {
            copyFrom(&other);
            return *this;
        }

        //Move
        const tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> & operator=( tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> && other ) throw()
        {
            moveFrom(&other);
            return *this;
        }

        //Copy
        inline void copyFrom( const tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> * const other ) throw()
        {
            //Move assignement operator called
            m_tiles         = std::vector< std::vector<tile_t> >( other->m_tiles.begin(), other->m_tiles.end() );
            m_totalNbPixels = other->m_totalNbPixels;
            m_pixelWidth    = other->m_pixelWidth;
            m_pixelHeight   = other->m_pixelHeight;
            m_nbTileColumns = other->m_nbTileColumns;
            m_nbTileRows    = other->m_nbTileRows;
        }

        //Move
        inline void moveFrom( tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> * other ) throw()
        {
            //Move
            m_tiles         = std::move_if_noexcept(other->m_tiles);
            m_totalNbPixels = other->m_totalNbPixels;
            m_pixelWidth    = other->m_pixelWidth;
            m_pixelHeight   = other->m_pixelHeight;
            m_nbTileColumns = other->m_nbTileColumns;
            m_nbTileRows    = other->m_nbTileRows;

            //Zero those out since we don't want the object to be left in a valid state!
            other->m_totalNbPixels = 0;
            other->m_pixelWidth    = 0;
            other->m_pixelHeight   = 0;
            other->m_nbTileColumns = 0;
            other->m_nbTileRows    = 0;
        }

        //------ Methods ------
        //Access the image data like a linear 1D array
        inline pixel_t & operator[]( unsigned int pos )
        {
            unsigned int tileIndex = pos       / tile_t::NB_PIXELS, //the Nth tile in our 2D vector, counting from 0, left to right.
                         tileRow   = tileIndex / m_nbTileColumns,   //Divide the nb of tiles by the current size of a row to get the cur row
                         tileCol   = tileIndex % m_nbTileColumns,   //Divide the nb of tiles by the current size of a row, and keep only the remaining value to get the cur row
                         pixRem    = pos       % tile_t::NB_PIXELS; //The nb of pixels in the cur tile before the position we want

            return m_tiles[tileRow][tileCol][pixRem];
        }

        //Access the image data like a linear 1D array
        inline const pixel_t & operator[]( unsigned int pos )const
        {
            return (*const_cast<tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> *>(this))[pos];
        }

        //Access the image data like a 2D bitmap
        inline pixel_t & getPixel( unsigned int x, unsigned int y )
        {
            unsigned int nbtilesonwidth     = x / tile_t::WIDTH,
                         pixeloffsetintileX = x % tile_t::WIDTH,
                         nbtilesonheight    = y / tile_t::HEIGHT,
                         pixeloffsetintileY = y % tile_t::HEIGHT;

            return m_tiles[nbtilesonheight][nbtilesonwidth].getPixel( pixeloffsetintileX, pixeloffsetintileY );
        }

        //Access the image data like a 2D bitmap
        inline const pixel_t & getPixel( unsigned int x, unsigned int y )const
        {
            return const_cast<tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> *>(this)->getPixel(x,y);
        }

        //Access a single tile via row and column coordinate
        inline tile_t & getTile( unsigned int col, unsigned int row )             
        { return m_tiles[row][col]; }

        //Access a single tile via row and column coordinate
        inline const tile_t & getTile( unsigned int col, unsigned int row ) const 
        { return m_tiles[row][col]; }

        //Access a single tile via tile index
        inline tile_t & getTile( unsigned int index )
        {
            unsigned int row = index / m_nbTileColumns,
                         col = index % m_nbTileColumns;

            return m_tiles[row][col];
        }

        //Access a single tile via tile index
        inline const tile_t & getTile( unsigned int index )const
        {
            return const_cast<tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS> *>(this)->getTile(index);
        }

        //Set the nb of tiles columns and tiles rows
        inline void setNbTilesRowsAndColumns( unsigned int nbcols, unsigned int nbrows )
        {
            m_tiles.resize( nbrows, std::vector<tile_t>(nbcols) );
            unsigned int rowwidth = (!m_tiles.empty())? m_tiles.front().size() : 0;

            m_totalNbPixels = tile_t::NB_PIXELS * (nbrows * nbcols);
            m_pixelHeight   = tile_t::HEIGHT    * m_tiles.size();
            m_pixelWidth    = tile_t::WIDTH     * rowwidth;
            m_nbTileColumns = rowwidth;
            m_nbTileRows    = m_tiles.size();
        }

        //Set the image resolution in pixels. Must be divisible by 8!
        inline void setPixelResolution( unsigned int pixelsWidth, unsigned int pixelsHeigth )
        {
            setNbTilesRowsAndColumns( pixelsHeigth / tile_t::HEIGHT, pixelsWidth / tile_t::WIDTH );
        }

        //Implementation for Non-indexed images
        inline virtual colorRGB24 getPixelRGBColor( unsigned int x, unsigned int y )const
        {
            return getPixel(x,y).ConvertToRGBColor();
        }

        //Implementation for Non-indexed images
        inline virtual colorRGB24 getPixelRGBColor( unsigned int linearpixelindex )const
        {
            return (*this)[linearpixelindex].ConvertToRGBColor();
        }

        //Get sizes and stuff
        inline unsigned int getNbRows()const        { return m_nbTileRows;    }
        inline unsigned int getNbCol()const         { return m_nbTileColumns; }
        inline unsigned int getNbPixelWidth()const  { return m_pixelWidth;    }
        inline unsigned int getNbPixelHeight()const { return m_pixelHeight;   }
        inline unsigned int getTotalNbPixels()const { return m_totalNbPixels; }
        inline unsigned int size()const             { return getTotalNbPixels(); } //Size, in PIXELS. For iterator and etc..

        //This returns the exact amount of bits that each pixels in the image uses
        inline unsigned int getSizeInBits()const    { return (getNbPixelWidth() * getNbPixelHeight()) * pixel_t::GetBitsPerPixel(); }

        //Access to the iterators
        inline iterator       begin() throw()       { return iterator(this,0); }
        inline const_iterator begin() const throw() { return const_iterator(this,0); }
        inline iterator       end()   throw()       { return iterator(const_cast<tiled_image<_PIXEL_T, _TILE_NB_ROWS,_TILE_NB_COLS>* >(this),m_totalNbPixels); }
        inline const_iterator end()   const throw() { return const_iterator(this,m_totalNbPixels); }

    protected:
        std::vector< std::vector<tile_t> > m_tiles;
        
        //This is to avoid recomputing those all the time, or dereferencing stuff to get the width and etc ! Its a real waste of time..
        unsigned int                       m_totalNbPixels,
                                           m_pixelWidth,
                                           m_pixelHeight,
                                           m_nbTileColumns,
                                           m_nbTileRows;
    };

//=============================================================================
// Tiled Indexed Image
//=============================================================================

    /*************************************************************************************************
        tiled_indexed_image
            Same as tiled_image, except that this one includes facilities to store a palette/colormap.
            And it also allows to get the color of a pixel directly, by only specifying its X/Y 
            coordinate!
    *************************************************************************************************/
    template< class _PIXEL_T, class _COLOR_T, unsigned int _TILE_NB_ROWS = 8, unsigned int _TILE_NB_COLS = 8 >
        class tiled_indexed_image : public tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>
    {
    public:
        static_assert( _PIXEL_T::mypixeltrait_t::IS_INDEXED, "Using a non-indexed pixel type inside a tiled_indexed_image is not allowed!" );
        typedef _COLOR_T pal_color_t;

        //------ Constructors ------
        tiled_indexed_image()
            :tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>(), m_palette(pixel_t::mypixeltrait_t::MAX_VALUE_PER_COMPONEMENT)
        {}

        tiled_indexed_image( unsigned int pixelsWidth, unsigned int pixelsHeigth )
            :tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>(pixelsWidth,pixelsHeigth), m_palette(pixel_t::mypixeltrait_t::MAX_VALUE_PER_COMPONEMENT)
        {}

        tiled_indexed_image( unsigned int pixelsWidth, unsigned int pixelsHeigth, unsigned int nbcolors )
            :tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>(pixelsWidth,pixelsHeigth), m_palette(nbcolors)
        {}

        tiled_indexed_image( const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> & other )
        {
            //do copy here
            copyFrom(&other);
        }

        tiled_indexed_image( tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> && other )
        {
            //do move here
            moveFrom(&other);
        }

        //Copy
        inline const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> & operator=( const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> & other )
        {
            copyFrom(&other);
            return *this;
        }

        //Move
        inline const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> & operator=( tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> && other )
        {
            moveFrom(&other);
            return *this;
        }

        inline void copyFrom(const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> * const other) 
        {
            m_palette = std::vector<pal_color_t>(other.m_palette.begin(), other.m_palette.end());
            //Call copyFrom from parent
            tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>::copyFrom( dynamic_cast<const tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> * const>(other) );
        }

        inline void moveFrom(tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> * other)
        {
            m_palette = std::move_if_noexcept(other->m_palette);
            //Call moveFrom from parent
            tiled_image<_PIXEL_T, _TILE_NB_ROWS, _TILE_NB_COLS>::moveFrom( dynamic_cast<tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS>*>(other) );
        }

        //------ Methods ------
        //Accessors
        inline std::vector<pal_color_t>       & getPalette()                                         { return m_palette; }
        inline const std::vector<pal_color_t> & getPalette()const                                    { return m_palette; }
        inline const pal_color_t              & getColor( typename pixel_t::pixeldata_t index )const { return m_palette[index]; }
        inline pal_color_t                    & getColor( typename pixel_t::pixeldata_t index )      { return m_palette[index]; }
        inline unsigned int                     getNbColors()const                                   { return m_palette.size(); }
        inline void                             setNbColors(unsigned int nbcol)                      { return m_palette.resize(nbcol); }
        inline void setColor( typename pixel_t::pixeldata_t index, pal_color_t && color )            { m_palette[index] = color; }
        inline void setColor( typename pixel_t::pixeldata_t index, pal_color_t color )               { m_palette[index] = std::move(color); }



        //Get the color of a pixel at (X, Y) directly
        inline pal_color_t & getPixelColorFromPalette( unsigned int x, unsigned int y )
        {
            return getColor( getPixel(x,y) );
        }

        inline const pal_color_t & getPixelColorFromPalette( unsigned int x, unsigned int y )const
        {
            return const_cast<tiled_indexed_image<_PIXEL_T,_COLOR_T,_TILE_NB_ROWS,_TILE_NB_COLS> *>(this)->getPixelColorFromPalette(x,y);
        }

        inline virtual colorRGB24 getPixelRGBColor( unsigned int x, unsigned int y )const
        {
            return getPixel(x,y).ConvertToRGBColor();
        }

        //Implementation for Non-indexed images
        inline virtual colorRGB24 getPixelRGBColor( unsigned int linearpixelindex )const
        {
            return (*this)[linearpixelindex].ConvertToRGBColor();
        }

    private:
        std::vector<pal_color_t> m_palette;
    };


//=============================================================================
// Tiled Multipalette Image
//=============================================================================
//Image contained in BGP files. They have a single palette per tile.



//=============================================================================
//Common tiled_image Typedefs
//=============================================================================
    typedef tiled_indexed_image<pixel_indexed_4bpp, colorRGB24> tiled_image_i4bpp; //Indexed 4bpp image with rgb24 palette.
    typedef tiled_indexed_image<pixel_indexed_8bpp, colorRGB24> tiled_image_i8bpp; //Indexed 8bpp image with rgb24 palette.

//=============================================================================
// Function Parse Image
//=============================================================================
    /*************************************************************************************************
        ParseTiledImg
            This function parses all 3 types of tiled_image. It only handles the pixels, not the 
            color palettes.
            Pass it iterators to a container containing raw bytes.

            - invertpixelorder : If true, reverse the pixel order on pixels smaller than 
                                 a single byte!

                                 If false, read higher pixels bits first, then lower ones. If true,
                                 reads lower pixels bits first, then higher ones.
                                 Basically, false is big endian, and true is little endian!

                                 For example, a 4bpp image would have its low nybble pixel read 
                                 first, if invertpixelorder was set to true !

    *************************************************************************************************/
    template<class _TILED_IMG_T, class _init>
        void ParseTiledImg( _init                itBegByte, 
                            _init                itEndByte,
                            utils::Resolution    imgrespixels,
                            _TILED_IMG_T        &out_img,
                            bool                 invertpixelorder = false )
    {
        //--> Inverting pixel order on pixels that overflow over several bytes isn't supported right now !! <--
        if( invertpixelorder )
        {
            assert( ( 8u % _TILED_IMG_T::pixel_t::GetBitsPerPixel() ) == 0 ); //#TODO: Specialize the temtplate when needed!
        }

        typedef _TILED_IMG_T                  image_t;
        typedef typename image_t::pixel_t     pixel_t;
        typedef typename pixel_t::pixeldata_t pixeldata_t;

        const unsigned int    NB_BITS_PER_PIXELS     = pixel_t::GetBitsPerPixel();
        const unsigned int    NB_BITS_IN_PIXEL_BYTES = pixel_t::GetBytesPerPixel() * 8u;
        const unsigned int    nboverlappingbits      = NB_BITS_IN_PIXEL_BYTES % NB_BITS_PER_PIXELS; 
        const unsigned int    NB_BYTES_INPUT         = std::distance( itBegByte, itEndByte ); //How much bytes we got to handle
        const unsigned int    NB_TOTAL_BITS_IMG      = (imgrespixels.width * imgrespixels.height) * NB_BITS_PER_PIXELS;
        
        //Check if everything is ok
        if( (NB_BYTES_INPUT * 8 != NB_TOTAL_BITS_IMG) && (NB_TOTAL_BITS_IMG > NB_BYTES_INPUT * 8) )
        {
            //The dimensions specified are too large for the data we got to read !
            assert(false);
            throw std::out_of_range("ParseTiledImg() : Image resolution too big for the amount of data provided !");
        }

        //auto               itCur          = itBegByte;
        //unsigned int       cptoutputimg   = 0;  
        out_img.setPixelResolution( imgrespixels.width, imgrespixels.height );


        auto         itpixel    = out_img.begin(), //Pixels contain ONLY the bits for a single pixel, not those of the adjacents ones!
                     itendpixel = out_img.end();
        unsigned int cptbits    = 0;

        //Handle every bits
        // Since we don't know if pixels are laid out across several bytes or whether they're aligned to 8 bits, 
        // we have to handle bit per bit !
        while( cptbits < NB_TOTAL_BITS_IMG && 
               itpixel != itendpixel && 
               itBegByte   != itEndByte )
        {
            unsigned int posbitinbyte        = ( cptbits % 8u );                 //0-7
            unsigned int posbitinpixel       = ( cptbits % NB_BITS_PER_PIXELS ); //0-(NB_BITS_PER_PIXELS-1)
            unsigned int ammounttoshiftright = 0;
            pixeldata_t  pixbitmask          = 0;

            if( invertpixelorder )
            {
                //Ex : For a 4bpp byte, we get the low nybble first, then the high
                ammounttoshiftright = posbitinbyte - posbitinpixel;
                pixbitmask          = ( 1u << ((NB_BITS_PER_PIXELS - posbitinpixel)-1) ); //The mask to keep only the bit we care about in the pixel
            }
            else
            {
                //Ex : For a 4bpp byte, we get the high nybble first, then the low
                ammounttoshiftright = ( 7u - posbitinbyte  ) - ( (NB_BITS_PER_PIXELS-1) - posbitinpixel );
                pixbitmask          = ( 1u << ((NB_BITS_PER_PIXELS - posbitinpixel)-1) ); //The mask to keep only the bit we care about in the pixel
            }

            (*itpixel) |= ( *itBegByte >> ammounttoshiftright ) & pixbitmask ; //BitwiseOR each bits one by one

            ++cptbits;
            if( cptbits!= 0 && cptbits % NB_BITS_PER_PIXELS == 0 ) //Increment the pixel iterator once we handled all bits for that pixel
            {
                ++itpixel;
                if( itpixel != itendpixel )
                    (*itpixel) = 0; //Its really important to make sure that the output byte is zeroed out
            }
            if( cptbits!= 0 &&  cptbits % 8u == 0 ) //Increment output iterator every 8 bits, to get a new byte to read from
                ++itBegByte;
        }
    }

    /*************************************************************************************************
        WriteTiledImg
            This function writes all 3 types of tiled_image to their tiled form into the target container. 
            It only handles the pixels, not the color palettes.
            Its meant to handle pixels that aren't aligned on 8 bits transparently.

            - invertpixelorder : If true, reverse the pixel order on pixels smaller than 
                                 a single byte!
                                 If true, writes lower pixel bits first, and then higher pixel bits.
                                 If false, writes the higher bits first, and then the lower bits.

    *************************************************************************************************/
    template<class _TILED_IMG_T, class _outit>
        void WriteTiledImg( _outit itBegByte, _outit itEndByte, const _TILED_IMG_T & img, bool invertpixelorder = false )
    {
        //--> Inverting pixel order on pixels that overflow over several bytes isn't supported right now !! <--
        if( invertpixelorder )
        {
            assert( ( 8u % _TILED_IMG_T::pixel_t::GetBitsPerPixel() ) == 0 ); //#TODO: Specialize the temtplate when needed!
        }

        typedef _TILED_IMG_T                  image_t;
        typedef typename image_t::pixel_t     pixel_t;
        typedef typename pixel_t::pixeldata_t pixeldata_t;

        const unsigned int    NB_BITS_PER_PIXELS          = pixel_t::GetBitsPerPixel();
        const unsigned int    NB_TOTAL_BITS_IMG           = img.getSizeInBits();
        const unsigned int    NB_AVAILABLE_BYTES_OUT      = std::distance(itBegByte,itEndByte);

        if( NB_AVAILABLE_BYTES_OUT * 8u < NB_TOTAL_BITS_IMG )
        {
            //Not enough bytes available to write out the entire image !
            assert(false);
            throw std::out_of_range("WriteTiledImg() : Output range too small to contain image !");
        }

        //Get some iterators on the image
        auto         itpixel    = img.begin(), //Pixels contain ONLY the bits for a single pixel, not those of the adjacents ones!
                     itendpixel = img.end();
        unsigned int cptbits    = 0;

        //Handle every bits
        // Since we don't know if pixels are laid out across several bytes or whether they're aligned to 8 bits, 
        // we have to handle bit per bit !
        while( cptbits < NB_TOTAL_BITS_IMG && 
               itpixel != itendpixel && 
               itBegByte   != itEndByte )
        {
            unsigned int posbitinbyte  = ( cptbits % 8u );                 //0-7
            unsigned int pixelbitindex = cptbits % NB_BITS_PER_PIXELS;     //0-(NB_BITS_PER_PIXELS-1)
            pixeldata_t  requiredshift = 0;

            if( invertpixelorder )
            {
                //Ex : For a 4bpp byte, we write the low nybble first, then the high
                requiredshift = posbitinbyte - pixelbitindex;
                // 0 1 2 3| 0 1 2 3  <- pixelbitindex
                // 0 1 2 3| 4 5 6 7  <- posbitinbyte
                // 0 0 0 0| 4 4 4 4  <- requiredshift
                // low    |   high  << Resulting order
                // 7 6 5 4| 3 2 1 0 << Bits numbered in the order they were read
            }
            else
            {
                //Ex : For a 4bpp byte, we write the high nybble first, then the low
                requiredshift = (7u - posbitinbyte) - ( (NB_BITS_PER_PIXELS-1u) - pixelbitindex );

                // 7 6 5 4| 3 2 1 0  <- pixelbitindex
                // 3 2 1 0| 3 2 1 0  <- posbitinbyte
                // 4 4 4 4| 0 0 0 0  <- requiredshift
                // high   |   low   << Resulting order
                // 0 1 2 3| 4 5 6 7 << Bits numbered in the order they were read
            }

            //Bits are numbered this way:
            //    0123 4567...
            (*itBegByte) |= (itpixel->getASingleBitInPlace(pixelbitindex) << requiredshift); //BitwiseOR each bits one by one

            ++cptbits;
            if( cptbits!= 0 && cptbits % NB_BITS_PER_PIXELS == 0 ) //Increment the pixel iterator once we handled all bits for that pixel
                ++itpixel;
            if( cptbits!= 0 &&  cptbits % 8u == 0 ) //Increment output iterator every 8 bits, to get a new byte to write to
            {
                ++itBegByte;
                if( itBegByte != itEndByte )
                    (*itBegByte) = 0; //Its really important to make sure that the output byte is zeroed out
            }
        }
    }

    /*************************************************************************************************
        WriteTiledImg
            Same as above, but for a back insert iterator

            - invertpixelorder : If true, reverse the pixel order on pixels smaller than 
                                 a single byte!
    *************************************************************************************************/
    template<class _TILED_IMG_T, class _backinsertit>
        void WriteTiledImg( _backinsertit itWhere, const _TILED_IMG_T & img, bool invertpixelorder = false )
    {
        //--> Inverting pixel order on pixels that overflow over several bytes isn't supported right now !! <--
        if( invertpixelorder )
        {
            assert( ( 8u % _TILED_IMG_T::pixel_t::GetBitsPerPixel() ) == 0 ); //#TODO: Specialize the temtplate when needed!
        }

        typedef _TILED_IMG_T                  image_t;
        typedef typename image_t::pixel_t     pixel_t;
        typedef typename pixel_t::pixeldata_t pixeldata_t;

        const unsigned int    NB_BITS_PER_PIXELS          = pixel_t::GetBitsPerPixel();
        const unsigned int    NB_TOTAL_BITS_IMG           = img.getSizeInBits();

        //Get some iterators on the image
        auto         itpixel    = img.begin(), //Pixels contain ONLY the bits for a single pixel, not those of the adjacents ones!
                     itendpixel = img.end();
        unsigned int cptbits    = 0;
        pixel_t      pixbuf     = 0; // this is a temporary pixel to assemble each pixel bit per bit.

        //Handle every bits
        // Since we don't know if pixels are laid out across several bytes or whether they're aligned to 8 bits, 
        // we have to handle bit per bit !
        while( ( cptbits < NB_TOTAL_BITS_IMG ) && ( itpixel != itendpixel ) )
        {
            unsigned int posbitinbyte  = ( cptbits % 8u );                 //0-7
            unsigned int pixelbitindex = cptbits % NB_BITS_PER_PIXELS;     //0-(NB_BITS_PER_PIXELS-1)
            pixeldata_t  requiredshift = 0;

            if( invertpixelorder )
            {
                //Ex : For a 4bpp byte, we write the low nybble first, then the high
                requiredshift = posbitinbyte - pixelbitindex;
                // 0 1 2 3| 0 1 2 3  <- pixelbitindex
                // 0 1 2 3| 4 5 6 7  <- posbitinbyte
                // 0 0 0 0| 4 4 4 4  <- requiredshift
                // low    |   high  << Resulting order
                // 7 6 5 4| 3 2 1 0 << Bits numbered in the order they were read
            }
            else
            {
                //Ex : For a 4bpp byte, we write the high nybble first, then the low
                requiredshift = (7u - posbitinbyte) - ( (NB_BITS_PER_PIXELS-1u) - pixelbitindex );

                // 7 6 5 4| 3 2 1 0  <- pixelbitindex
                // 3 2 1 0| 3 2 1 0  <- posbitinbyte
                // 4 4 4 4| 0 0 0 0  <- requiredshift
                // high   |   low   << Resulting order
                // 0 1 2 3| 4 5 6 7 << Bits numbered in the order they were read
            }

            //Bits are numbered this way:
            //    0123 4567...
            pixbuf |= (itpixel->getASingleBitInPlace(pixelbitindex) << requiredshift); //BitwiseOR each bits one by one



            ++cptbits;
            if( cptbits!= 0 && cptbits % NB_BITS_PER_PIXELS == 0 ) //Increment the pixel iterator once we handled all bits for that pixel
                ++itpixel;
            if( cptbits!= 0 &&  cptbits % 8u == 0 ) //Increment output iterator every 8 bits, to get a new byte to write to
            {
                *itWhere = pixbuf;
                ++itWhere;
                //Reset temp pixel content
                pixbuf = 0;
            }
        }
    }

//=============================================================================
// Function Output Image
//=============================================================================

    /*************************************************************************************************
        OutputRawImageAsTiled
            A function to output a tiled image in its raw form into a file!
            Its exported as a tiled image, preceeded by a palette if there is one!
            There are no header or delimiter in this file format its literally a dump!

            - invertpixelorder : If true, reverse the pixel order on pixels smaller than 
                                 a single byte!

    *************************************************************************************************/
    template<class _TILED_IMG_T>
        void OutputRawImageAsTiled( const _TILED_IMG_T & img, const std::string & filepath, bool invertpixelorder = false )
    {
        //--> Inverting pixel order on pixels that overflow over several bytes isn't supported right now !! <--
        if( invertpixelorder )
        {
            assert( ( 8u % _TILED_IMG_T::pixel_t::GetBitsPerPixel() ) == 0 ); //#TODO: Specialize the temtplate when needed!
        }

        std::vector<uint8_t> outputbuffer;
        unsigned int         bytestoreserve = (img.getSizeInBits() / 8u) + ( (img.getSizeInBits() % 8u != 0)? 1u : 0u );

        //If we have a palette to write too, factor it in the total size !
        if( _TILED_IMG_T::pixel_t::IsIndexedPixel() )
            bytestoreserve += img.getPalette().size() * _TILED_IMG_T::pal_color_t::getSizeRawBytes();

        outputbuffer.reserve( bytestoreserve );

        //#1 - Write the palette if the image has one !
        if( _TILED_IMG_T::pixel_t::IsIndexedPixel() )
        {
            const vector<typename _TILED_IMG_T::pal_color_t> & refpal   = img.getPalette();
            auto                                               itinsert = std::back_inserter( outputbuffer );

            for( auto & acolor : refpal )
                acolor.WriteAsRawByte( itinsert );
        }

        //Save the offset where the image data will begin!
        std::size_t offsetBegImg = outputbuffer.size();

        //Resize the buffer to its full size we allocated earlier
        outputbuffer.resize( bytestoreserve );

        //Make some iterators now that we don't need to resize anymore
        auto itbegimg = outputbuffer.begin() + offsetBegImg,
             itendimg = outputbuffer.begin() + bytestoreserve;

        //#2 - Write the pixels
        WriteTiledImg( itbegimg, itendimg, img, invertpixelorder );

        //#3 - Write the buffer!
        utils::WriteByteVectorToFile( filepath, outputbuffer );
    }
};

//#include "tiled_image.cpp"

#endif