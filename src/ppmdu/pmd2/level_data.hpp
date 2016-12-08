#ifndef LEVEL_DATA_HPP
#define LEVEL_DATA_HPP
/*
level_data.hpp
2016/11/11
psycommando@gmail.com
Description: Utilities for exporting and importing level data to and from folders containing XML.
*/
#include <ppmdu/pmd2/pmd2.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace pmd2
{
//
//
//
    /*
        LevelData
            Contains data about a single level from the PMD2 game!
    */
    class LevelData
    {
    public:

    private:
        std::string m_lvlname;
    };

    /*
        LevelsDB
            Contains all the data about several levels from the PMD2 game!
    */
    class LevelsDB
    {
    public:

    private:
        std::unordered_map<std::string,LevelData> m_lvldata; //First is lvlname, second is level data

    };

//
//
//
    /*
    */
    void ExportXMLLevels( const std::string    & destdir, 
                          const LevelsDB       & lvldata );

    /*
    */
    LevelsDB ImportXMLLevels( const std::vector<std::string> & lvldirpaths );

};

#endif
