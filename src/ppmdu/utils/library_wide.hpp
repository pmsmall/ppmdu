#ifndef LIBRARY_WIDE_HPP
#define LIBRARY_WIDE_HPP
/*
library_wide.hpp
2014/10/20
psycommando@gmail.com
Description: 
    Contains a singleton holding information relevant to how most of the library should behave.
    Basically, a form of clean "globals" !
*/
#include <ppmdu/basetypes.hpp>

namespace utils
{
    /***************************************************************
        lwData
            Values to be shared library wide.
            Stored inside a struct.
            This might eventually get more elaborate at one point.
    ***************************************************************/
    class lwData
    {
    public:
        lwData();

        //Verbose
        void setVerbose( bool ison );
        bool isVerboseOn()const;

        //Nb threads to use at most
        void setNbThreadsToUse( unsigned int nbthreads );
        unsigned int getNbThreadsToUse()const;

    private:
        bool         m_verboseOn;
        unsigned int m_nbThreads;
    };

    /***************************************************************
        LibraryWide
            Singleton that init the library-wide settings.
    ***************************************************************/
    class LibraryWide
    {
    public:
        static LibraryWide & getInstance();

        //
        lwData & Data();

    private:
        LibraryWide();
        LibraryWide( const LibraryWide & );
        LibraryWide & operator=( const LibraryWide & );

        lwData  m_data;
    };
};

#endif