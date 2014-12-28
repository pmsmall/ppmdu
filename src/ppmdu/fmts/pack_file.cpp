/*
*/
#include <ppmdu/fmts/pack_file.hpp>
#include <ppmdu/pmd2/pmd2_filetypes.hpp>
#include <ppmdu/fmts/content_type_analyser.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <Poco/DirectoryIterator.h>
#include <cassert>
#include <ppmdu/utils/gbyteutils.hpp>
#include <ppmdu/utils/utility.hpp>
#include <ppmdu/utils/utility.hpp>
using namespace std;
using namespace utils;

namespace pmd2 { namespace filetypes 
{
//===============================================================================
//                                  Functions
//===============================================================================

    //void WriteProgressConsole( int percent )
    //{
    //    std::cout << "\r" <<"   => Progress: " <<std::setw(3) <<std::dec <<percent <<"%";
    //}

    //void WriteProgressConsole( int percent, const string & textbefore )
    //{
    //    std::cout << "\r" <<textbefore <<std::setfill(' ') <<std::setw(4) <<std::dec <<percent  <<"%";
    //}

    uint32_t ComputeFileNBPaddingBytes( uint32_t filelen )
    {
        return ( GetNextInt32DivisibleBy16( filelen ) - filelen );
    }


//===============================================================================
// fileindex
//===============================================================================
    //uint8_t & fileIndex::operator[](unsigned int index)
    //{
    //    if( index < 4 )
    //        return reinterpret_cast<uint8_t*>(&_fileOffset)[index];
    //    else if( index < 8 )
    //        return reinterpret_cast<uint8_t*>(&_fileLength)[index-4];
    //    else
    //        return *reinterpret_cast<uint8_t*>(0); //Crash please
    //}

    //const uint8_t & fileIndex::operator[](unsigned int index)const
    //{
    //    return (*const_cast<fileIndex*>(this))[index];
    //}

    std::vector<uint8_t>::iterator fileIndex::WriteToContainer( std::vector<uint8_t>::iterator itwriteto )const
    {
        itwriteto = utils::WriteIntToByteVector( _fileOffset, itwriteto );
        itwriteto = utils::WriteIntToByteVector( _fileLength, itwriteto );
        return itwriteto;
    }

    std::vector<uint8_t>::const_iterator fileIndex::ReadFromContainer( std::vector<uint8_t>::const_iterator itReadfrom )
    {
        _fileOffset = utils::ReadIntFromByteVector<decltype(_fileOffset)>(itReadfrom);
        _fileLength = utils::ReadIntFromByteVector<decltype(_fileLength)>(itReadfrom);
        return itReadfrom;
    }

//===============================================================================
// pfheader
//===============================================================================

    //uint8_t & pfheader::operator[](unsigned int index)
    //{
    //    if( index < 4 )
    //        return reinterpret_cast<uint8_t*>(&_zeros)[index];
    //    else if( index < 8 )
    //        return reinterpret_cast<uint8_t*>(&_nbfiles)[index-4];
    //    else
    //        return *reinterpret_cast<uint8_t*>(0); //Crash please
    //}

    //const uint8_t & pfheader::operator[](unsigned int index)const
    //{
    //    return (*const_cast<pfheader*>(this))[index];
    //}

    std::vector<uint8_t>::iterator pfheader::WriteToContainer( std::vector<uint8_t>::iterator itwriteto )const
    {
        itwriteto = utils::WriteIntToByteVector( _zeros,   itwriteto );
        itwriteto = utils::WriteIntToByteVector( _nbfiles, itwriteto );
        return itwriteto;
    }

    std::vector<uint8_t>::const_iterator pfheader::ReadFromContainer( std::vector<uint8_t>::const_iterator itReadfrom )
    {
        _zeros   = utils::ReadIntFromByteVector<decltype(_zeros)>  (itReadfrom);
        _nbfiles = utils::ReadIntFromByteVector<decltype(_nbfiles)>(itReadfrom);
        return itReadfrom;
    }

    bool pfheader::isValid()const
    {
        return (_zeros == 0x0) && (_nbfiles > 0x0);
    }


//===============================================================================
//								CPack
//===============================================================================
    CPack::CPack()
        :m_ForcedFirstFileOffset(0)
    {

    }

    void CPack::ClearState()
    {
        //Clear all current data
        m_SubFiles.resize(0);
        m_OffsetTable.resize(0);
        m_ForcedFirstFileOffset = 0;
    }
    
    uint32_t CPack::PredictHeaderSize( uint32_t nbsubfiles )
    {
        // Count the begining zeros and the size of the entry for the nb of files 
        uint32_t headerlengthsofar = pfheader::HEADER_LEN;

        //calculate the number of file offset table entries by multiplying nb of files by the size of each entries
        headerlengthsofar += ( nbsubfiles * SZ_OFFSET_TBL_ENTRY );

        //Add the end delemiter 
        headerlengthsofar += SZ_OFFSET_TBL_DELIM; 

        return headerlengthsofar;
    }

    uint32_t CPack::PredictHeaderSizeWithPadding( uint32_t nbsubfiles )
    {
        return GetNextInt32DivisibleBy16( PredictHeaderSize(nbsubfiles) );
    }

    uint32_t CPack::getCurrentPredictedHeaderLengthWithForcedOffset()const
    {
        return ( IsForcedOffsetCurrentlyPossible() ) ? m_ForcedFirstFileOffset : PredictHeaderSizeWithPadding(m_SubFiles.size()); //It recalculates a lot !
    }

    bool CPack::IsForcedOffsetCurrentlyPossible()const
    {
        return ( m_ForcedFirstFileOffset > PredictHeaderSizeWithPadding( m_SubFiles.size() ) );
    }

    void CPack::LoadPack( types::constitbyte_t beg, types::constitbyte_t end )
    {
        utils::MrChronometer chronopacker( "PackFile Loader" );

        //Clear all current data
        ClearState();

        //#1 - Read header
        pfheader mahead;
        mahead.ReadFromContainer(beg);

		//----------- Analyze file ----------
        //Is it using a forced first file offset ?
        m_ForcedFirstFileOffset = IsPackFileUsingForcedFFOffset( beg, mahead._nbfiles );

        //if( m_ForcedFirstFileOffset > 0 )
        //    cout << " !-Extra padding detected ! First file offset is forced to offset 0x" 
        //         << std::hex << m_ForcedFirstFileOffset <<std::dec <<" !\n";

        //Fill the File Offset Table
        ReadFOTFromPackFile( beg, mahead._nbfiles );

        //Get the file data
        ReadSubFilesFromPackFileUsingFOT( beg );
    }


    void CPack::LoadFolder( const std::string & pathdir )
    {
        utils::MrChronometer chronofolderloader("Folder Loader");

        //Check folder exists
        if( !utils::isFolder(pathdir) )
            throw runtime_error("<!>-Error: Invalid input path !");

        //Clear current data, reset forced first file offset to 0
        ClearState();

        //List files in folder, put them into a vector
        Poco::DirectoryIterator itdir(pathdir),
                                itdirend;
        unsigned int            nbfiles = 0;

        //A little lambda for determining valid files
        static auto lambdavalidfile = []( const Poco::File & f )
        { 
            return f.isFile() && !(f.isHidden()); 
        };

        //Count valid files inside folder
        for( auto itcount = itdir; itcount != itdirend; ++itcount )
        {
            if( lambdavalidfile(*itcount) )
                ++nbfiles;
        }

        m_SubFiles.resize(nbfiles);
        
        //Read each files into the file data table
        for( unsigned int cptfiles = 0; itdir != itdirend; ++cptfiles, ++itdir )
        {
            if( lambdavalidfile(*itdir) )
                utils::ReadFileToByteVector( itdir->path(), m_SubFiles[cptfiles] );
        }

    }


    //void CPack::OutputToFile( const std::string & pathfile )
    types::bytevec_t CPack::OutputPack()
    {
        utils::MrChronometer chronotwat("Writing Pack File");

        //Build the FOT
        BuildFOT();

        types::bytevec_t result( PredictTotalFileSize() );
        types::itbyte_t  ittwritepos = WriteFullHeader( result.begin() );

        ittwritepos = WriteFileData( ittwritepos );

        return std::move(result); //Implicit move constructor
    }


    void CPack::OutputToFolder( const std::string & pathdir )
    {
        MrChronometer chronooutputer( "Unpacking Files" );

        if( !utils::DoCreateDirectory( pathdir ) )
            throw exception("Invalid output path!");

        //write them out
        for( unsigned int i = 0; i < m_SubFiles.size(); ++i)
            WriteSubFileToFile( m_SubFiles[i], pathdir, i );
    }


    void CPack::BuildFOT()
    {
        //Empty FOT first
        m_OffsetTable.resize(0);

        //Computer the header's length
        uint32_t offsetsofar = getCurrentPredictedHeaderLengthWithForcedOffset();
        m_OffsetTable.reserve( m_SubFiles.size() );

        //Add files to the offset table and !! compute padding for each to get the correct offsets !!
        for( auto & entry : m_SubFiles )
        {
            m_OffsetTable.push_back( fileIndex( offsetsofar, entry.size() ) );
            offsetsofar += entry.size();
            offsetsofar =  GetNextInt32DivisibleBy16( offsetsofar ); //compensate for padding
        }
    }

    uint32_t CPack::CalcAmountHeaderPaddingBytes()const
    {
        assert( !m_OffsetTable.empty() );
        uint32_t headerlengthwithpadding = PredictHeaderSizeWithPadding( m_OffsetTable.size() );
        
        if( m_ForcedFirstFileOffset > headerlengthwithpadding ) //Calculate the extra forced padding
            headerlengthwithpadding = m_ForcedFirstFileOffset;

        return headerlengthwithpadding - PredictHeaderSize( m_OffsetTable.size() );
    }

    types::constitbyte_t CPack::ReadHeader( types::constitbyte_t itbegin, pfheader & out_mahead )const
    {
        return out_mahead.ReadFromContainer(itbegin);
    }

    // !!- OK -!!
    void CPack::ReadFOTFromPackFile( types::constitbyte_t itbegin, unsigned int nbsubfiles )
    {
        const uint32_t       TOTAL_BYTES_FOT = nbsubfiles * SZ_OFFSET_TBL_ENTRY;
        types::constitbyte_t itt             = itbegin;
        m_OffsetTable.resize( nbsubfiles );

        std::advance(    itt,  OFFSET_TBL_FIRST_ENTRY );                    //Move to beginning of FOT
        //std::advance( ittend, (TOTAL_BYTES_FOT + OFFSET_TBL_FIRST_ENTRY) ); //move end iterator to end of FOT

        for( auto & entry : m_OffsetTable )
        {
            fileIndex fi;
            itt = fi.ReadFromContainer(itt);
            entry = fi;
        }
    }


    void CPack::ReadSubFilesFromPackFileUsingFOT( types::constitbyte_t itbegin )
    {
        assert( !m_OffsetTable.empty() ); //If this happens, the method was probably called before the FOT was built..
        const auto NB_SUBFILES = m_OffsetTable.size(); //Avoid doing function calls all the time, and also allow compiler optimization for constants
        m_SubFiles.resize( NB_SUBFILES );

        //cout << " -Reading subfiles..\n";

        for( unsigned int i = 0; i < NB_SUBFILES; ++i )
        {
            //Resize the destination container
            m_SubFiles[i].resize( m_OffsetTable[i]._fileLength );

            //Copy the data to it
            copy_n( itbegin + m_OffsetTable[i]._fileOffset, m_OffsetTable[i]._fileLength, m_SubFiles[i].begin() );
        }
    }

    // !!- OK -!!
    uint32_t CPack::IsPackFileUsingForcedFFOffset( types::constitbyte_t itbeg, unsigned int nbsubfiles )const
    {
        uint32_t             expectedlength;
        uint32_t             actuallength;
        types::constitbyte_t ittread        = itbeg;
        
        expectedlength = PredictHeaderSizeWithPadding( nbsubfiles ); //compute expected header length

        //Get the actual first file offset from the file
        std::advance( ittread, OFFSET_TBL_FIRST_ENTRY );

        actuallength = utils::ReadIntFromByteVector<uint32_t>(ittread);

        //compare with first subfile's offset in file's offset table
        return ( actuallength > expectedlength ) ? actuallength : 0;
    }

    // !!- OK -!!
    uint32_t CPack::PredictTotalFileSize()const
    {
        uint32_t filesizesofar = getCurrentPredictedHeaderLengthWithForcedOffset();

        for( auto &subfile : m_SubFiles )
            filesizesofar = GetNextInt32DivisibleBy16( subfile.size() + filesizesofar );

        return filesizesofar;
    }

    // !!- OK -!!
    std::vector<uint8_t>::iterator CPack::WriteFullHeader( std::vector<uint8_t>::iterator writeat )const
    {
        assert( !m_OffsetTable.empty() );
        pfheader mahead;
        mahead._zeros   = 0;
        mahead._nbfiles = m_SubFiles.size();

        writeat = mahead.WriteToContainer( writeat );

        //Write FOT
        for( auto &fotentry : m_OffsetTable )
            writeat = fotentry.WriteToContainer(writeat);

        //Write FOT end delimiter
        writeat = std::copy( OFFSET_TBL_DELIM.begin(), OFFSET_TBL_DELIM.end(), writeat );

        //Write padding bytes
        writeat = std::fill_n( writeat, CalcAmountHeaderPaddingBytes(), PF_PADDING_BYTE );

        return writeat;
    }

    types::itbyte_t CPack::WriteFileData( types::itbyte_t writeat )
    {
        //Add file data, and add padding after those that need it !
        for( auto & afile : m_SubFiles )
        {
            writeat = copy( afile.begin(), afile.end(), writeat );

            //Write padding
            writeat = fill_n( writeat, 
                              ComputeFileNBPaddingBytes( afile.size() ), 
                              PF_PADDING_BYTE );
        }

        return writeat;
    }

    string SubfileGetFExtension( types::constitbyte_t beg, types::constitbyte_t end )
    {
        string result = GetAppropriateFileExtension(beg,end);
        if( result.empty() )
            return std::move( result );
        else
            return "." + result;
    }

    void CPack::WriteSubFileToFile( types::bytevec_t  & file,
                                    const std::string & path, 
                                    unsigned int        fileindex )
    {
        static const string FILE_PREFIX = "file_";
		//----- 1. Make output filename -----
		stringstream     outfilename;

        outfilename << utils::AppendTraillingSlashIfNotThere( path ) << FILE_PREFIX
                    <<std::setfill('0') <<std::setw(4) <<std::dec <<fileindex
                    << SubfileGetFExtension( file.begin(), file.end() );

        

        //itput = std::copy( FILE_PREFIX.begin(), FILE_PREFIX.end(), itput );
        //outfilename <<std::setfill('0') <<std::setw(4) <<std::dec <<fileindex;
        
        //string etemp = outfilename.str();

        //itput = std::copy( etemp.begin(), etemp.end(), itput );

        //(*itput) = '.';
        //++itput;

        //string fextension = GetAppropriateFileExtension( file.begin(), file.end() );
        //if( !fextension.empty() )
        //    itput = std::copy( fextension.begin(), fextension.end(), itput );

        //outfilename <<"file_" <<std::setfill('0') <<std::setw(4) <<std::dec <<fileindex;  //<<"_offs_0x" 
                    //<<std::setfill('0') <<std::setw(8) <<std::hex << m_OffsetTable[fileindex]._fileOffset;

		//if we have a valid extension append it
        //string fextension = GetAppropriateFileExtension( file.begin(), file.end() );
   //     if( !fextension.empty() )
			//outfilename << "." << fextension;

		//------- 2. Output -------
        WriteByteVectorToFile( outfilename.str(), file ); 
    }

//========================================================================================================
//  packfile_rule
//========================================================================================================

    /*
        packfile_rule
            Rule for identifying a pack file.
    */
    class packfile_rule : public IContentHandlingRule
    {
    public:
        packfile_rule(){}
        ~packfile_rule(){}

        //Returns the value from the content type enum to represent what this container contains!
        virtual e_ContentType getContentType()const;

        //Returns an ID number identifying the rule. Its not the index in the storage array,
        // because rules can me added and removed during exec. Thus the need for unique IDs.
        //IDs are assigned on registration of the rule by the handler.
        content_rule_id_t getRuleID()const;
        void              setRuleID( content_rule_id_t id );

        //This method returns the content details about what is in-between "itdatabeg" and "itdataend".
        //## This method will call "CContentHandler::AnalyseContent()" for each sub-content container found! ##
        //virtual ContentBlock Analyse( types::constitbyte_t   itdatabeg, 
        //                              types::constitbyte_t   itdataend );
        virtual ContentBlock Analyse( const filetypes::analysis_parameter & parameters );

        //This method is a quick boolean test to determine quickly if this content handling
        // rule matches, without in-depth analysis.
        virtual bool isMatch(  types::constitbyte_t   itdatabeg, 
                               types::constitbyte_t   itdataend,
                               const std::string    & filext);

    private:
        content_rule_id_t m_myID;
    };


    //Returns the value from the content type enum to represent what this container contains!
    e_ContentType packfile_rule::getContentType()const
    {
        return e_ContentType::PACK_CONTAINER;
    }

    //Returns an ID number identifying the rule. Its not the index in the storage array,
    // because rules can me added and removed during exec. Thus the need for unique IDs.
    //IDs are assigned on registration of the rule by the handler.
    content_rule_id_t packfile_rule::getRuleID()const
    {
        return m_myID;
    }
    void packfile_rule::setRuleID( content_rule_id_t id )
    {
        m_myID = id;
    }

    //This method returns the content details about what is in-between "itdatabeg" and "itdataend".
    //## This method will call "CContentHandler::AnalyseContent()" for each sub-content container found! ##
    ContentBlock packfile_rule::Analyse( const filetypes::analysis_parameter & parameters )
    {
        pfheader     headr;
        ContentBlock cb;

        //Read the header
        headr.ReadFromContainer( parameters._itdatabeg );

        //Get the last entry in the FOT and add its len to its offset to get the end offset!
        auto itlastentry = parameters._itdatabeg;
        advance( itlastentry, OFFSET_TBL_FIRST_ENTRY + ( headr._nbfiles * SZ_OFFSET_TBL_ENTRY ) );

        fileIndex lastentry;
        lastentry.ReadFromContainer( itlastentry );

        //build our content block info
        cb._startoffset          = 0;
        cb._endoffset            = lastentry._fileOffset + lastentry._fileLength;
        cb._rule_id_that_matched = getRuleID();
        cb._type                 = getContentType();


        //Try to guess what is the subcontent
        //analysis_parameter paramtopass( parameters._itparentbeg + headr.subheaderptr,  
        //                                parameters._itparentbeg + headr.eofptr, 
        //                                parameters._itparentbeg, 
        //                                parameters._itparentend );

        //ContentBlock subcontent = CContentHandler::GetInstance().AnalyseContent( paramtopass );
        //subcontent._startoffset = headr.subheaderptr; //Set the actual start offset!
        //cb._hierarchy.push_back( subcontent );

        return cb;
    }

    //This method is a quick boolean test to determine quickly if this content handling
    // rule matches, without in-depth analysis.
    bool packfile_rule::isMatch( types::constitbyte_t itdatabeg, types::constitbyte_t itdataend , const std::string & filext )
    {
        pfheader headr;
        itdatabeg = headr.ReadFromContainer( itdatabeg );
        uint32_t nextint = utils::ReadIntFromByteVector<uint32_t>(itdatabeg); //We know that the next value is the first entry in the ToC, if its really a pack file!
        
        bool     filextokornotthere = filext.compare( filetypes::PACK_FILEX ) == 0;

        return (headr._zeros == 0) && (headr._nbfiles > 0) && (nextint != 0);  //TODO: improve this, it fails and recognize files that aren't pack files !!
    }

//========================================================================================================
//  at4px_rule_registrator
//========================================================================================================
    /*
        at4px_rule_registrator
            A small singleton that has for only task to register the at4px_rule!
    */
    RuleRegistrator<packfile_rule> RuleRegistrator<packfile_rule>::s_instance;

};};