#include "audioutil.hpp"
#include <ppmdu/utils/utility.hpp>
#include <ppmdu/utils/cmdline_util.hpp>
#include <ppmdu/pmd2/pmd2_audio_data.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/Exception.h>

#include <jdksmidi/world.h>
#include <jdksmidi/track.h>
#include <jdksmidi/multitrack.h>
#include <jdksmidi/filereadmultitrack.h>
#include <jdksmidi/fileread.h>
#include <jdksmidi/fileshow.h>
#include <jdksmidi/filewritemultitrack.h>

using namespace ::utils::cmdl;
using namespace ::utils::io;
using namespace ::std;

namespace audioutil
{
//=================================================================================================
//  CAudioUtil
//=================================================================================================

//------------------------------------------------
//  Constants
//------------------------------------------------
    const string CAudioUtil::Exe_Name            = "ppmd_audioutil.exe";
    const string CAudioUtil::Title               = "Music and sound import/export tool.";
    const string CAudioUtil::Version             = "0.1";
    const string CAudioUtil::Short_Description   = "A utility to export and import music and sounds from the PMD2 games.";
    const string CAudioUtil::Long_Description    = 
        "#TODO";
    const string CAudioUtil::Misc_Text           = 
        "Named in honour of Baz, the awesome Poochyena of doom ! :D\n"
        "My tools in binary form are basically Creative Commons 0.\n"
        "Free to re-use in any ways you may want to!\n"
        "No crappyrights, all wrongs reversed! :3";

//
//
//
    //void WriteMusicDump( const pmd2::audio::MusicSequence & seq, const std::string & fname );
    void WriteEventsToMidiFileTest( const std::string & file, const pmd2::audio::MusicSequence & seq );
    void WriteEventsToMidiFileTest_MF( const std::string & file, const pmd2::audio::MusicSequence & seq );

//------------------------------------------------
//  Arguments Info
//------------------------------------------------
    /*
        Data for the automatic argument parser to work with.
    */
    const vector<argumentparsing_t> CAudioUtil::Arguments_List =
    {{
        //Input Path argument
        { 
            0,      //first arg
            false,  //false == mandatory
            true,   //guaranteed to appear in order
            "input path", 
            "Path to the file/directory to export, or the directory to assemble.",
#ifdef WIN32
            "\"c:/pmd_romdata/data.bin\"",
#elif __linux__
            "\"/pmd_romdata/data.bin\"",
#endif
            std::bind( &CAudioUtil::ParseInputPath, &GetInstance(), placeholders::_1 ),
        },
        //Output Path argument
        { 
            1,      //second arg
            true,   //true == optional
            true,   //guaranteed to appear in order
            "output path", 
            "Output path. The result of the operation will be placed, and named according to this path!",
#ifdef WIN32
            "\"c:/pmd_romdata/data\"",
#elif __linux__
            "\"/pmd_romdata/data\"",
#endif
            std::bind( &CAudioUtil::ParseOutputPath, &GetInstance(), placeholders::_1 ),
        },
    }};




//------------------------------------------------
//  Options Info
//------------------------------------------------

    /*
        Information on all the switches / options to allow the automated parser 
        to parse them.
    */
    const vector<optionparsing_t> CAudioUtil::Options_List=
    {{
        //Force Import
        //{
        //    "i",
        //    0,
        //    "Specifying this will force import!",
        //    "-i",
        //    std::bind( &CAudioUtil::ParseOptionForceImport, &GetInstance(), placeholders::_1 ),
        //},
    }};


//------------------------------------------------
// Misc Methods
//------------------------------------------------

    CAudioUtil & CAudioUtil::GetInstance()
    {
        static CAudioUtil s_util;
        return s_util;
    }

    CAudioUtil::CAudioUtil()
        :CommandLineUtility()
    {
        m_operationMode = eOpMode::Invalid;
    }

    const vector<argumentparsing_t> & CAudioUtil::getArgumentsList   ()const { return Arguments_List;    }
    const vector<optionparsing_t>   & CAudioUtil::getOptionsList     ()const { return Options_List;      }
    const argumentparsing_t         * CAudioUtil::getExtraArg        ()const { return nullptr;           } //No extra args
    const string                    & CAudioUtil::getTitle           ()const { return Title;             }
    const string                    & CAudioUtil::getExeName         ()const { return Exe_Name;          }
    const string                    & CAudioUtil::getVersionString   ()const { return Version;           }
    const string                    & CAudioUtil::getShortDescription()const { return Short_Description; }
    const string                    & CAudioUtil::getLongDescription ()const { return Long_Description;  }
    const string                    & CAudioUtil::getMiscSectionText ()const { return Misc_Text;         }

//--------------------------------------------
//  Utility
//--------------------------------------------



//--------------------------------------------
//  Parse Args
//--------------------------------------------
    bool CAudioUtil::ParseInputPath( const string & path )
    {
        Poco::File inputfile(path);

        //check if path exists
        if( inputfile.exists() && ( inputfile.isFile() || inputfile.isDirectory() ) )
        {
            m_inputPath = path;
            return true;
        }
        return false;
    }
    
    bool CAudioUtil::ParseOutputPath( const string & path )
    {
        Poco::Path outpath(path);

        if( outpath.isFile() || outpath.isDirectory() )
        {
            m_outputPath = path;
            return true;
        }
        return false;
    }


//
//  Parse Options
//

    //bool CAudioUtil::ParseOptionPk( const std::vector<std::string> & optdata )
    //{
    //}


//
//  Program Setup and Execution
//
    int CAudioUtil::GatherArgs( int argc, const char * argv[] )
    {
        int returnval = 0;
        //Parse arguments and options
        try
        {
            if( !SetArguments(argc,argv) )
                return -3;

            DetermineOperation();
        }
        catch( Poco::Exception pex )
        {
            cerr <<"\n<!>-POCO Exception - " <<pex.name() <<"(" <<pex.code() <<") : " << pex.message() <<"\n" <<endl;
            cout <<"=======================================================================\n"
                 <<"Readme\n"
                 <<"=======================================================================\n";
            PrintReadme();
            return pex.code();
        }
        catch( exception e )
        {
            cerr <<"\n<!>-Exception: " << e.what() <<"\n" <<endl;
            cout <<"=======================================================================\n"
                 <<"Readme\n"
                 <<"=======================================================================\n";
            PrintReadme();
            return -3;
        }
        return returnval;
    }

    void CAudioUtil::DetermineOperation()
    {
        Poco::Path inpath( m_inputPath );
        Poco::File infile( inpath );

        if( m_operationMode != eOpMode::Invalid )
            return; //Skip if we have a forced mode         

        if( !m_outputPath.empty() && !Poco::File( Poco::Path( m_outputPath ).makeAbsolute().parent() ).exists() )
            throw runtime_error("Specified output path does not exists!");

        if( infile.exists() )
        {
            if( infile.isFile() )
            {
                stringstream sstr;
                string fext = inpath.getExtension();
                std::transform(fext.begin(), fext.end(), fext.begin(), ::tolower);

                if( fext == pmd2::audio::SMDL_FileExtension )
                    m_operationMode = eOpMode::ExportSMDL;
                else if( fext == pmd2::audio::SEDL_FileExtension )
                    m_operationMode = eOpMode::ExportSEDL;
                else if( fext == pmd2::audio::SWDL_FileExtension )
                    m_operationMode = eOpMode::ExportSWDL;
                else
                    throw runtime_error("Can't import this file format!");
            }
            else if( infile.isDirectory() )
            {

                //if( m_hndlStrings )
                //{
                //    if( m_force == eOpForce::Export )
                //        m_operationMode = eOpMode::ExportGameStrings;
                //    else
                //        throw runtime_error("Can't import game strings from a directory : " + m_inputPath);
                //}
                //else if( m_hndlItems )
                //{
                //    if( m_force == eOpForce::Export )
                //        m_operationMode = eOpMode::ExportItemsData;
                //    else
                //        m_operationMode = eOpMode::ImportItemsData;
                //}
                //else if( m_hndlMoves )
                //{
                //    if( m_force == eOpForce::Export )
                //        m_operationMode = eOpMode::ExportMovesData;
                //    else
                //        m_operationMode = eOpMode::ImportMovesData;

                //}
                //else if( m_hndlPkmn )
                //{
                //    if( m_force == eOpForce::Export )
                //        m_operationMode = eOpMode::ExportPokemonData;
                //    else
                //        m_operationMode = eOpMode::ImportPokemonData;
                //}
                //else
                //{
                //    if( m_force == eOpForce::Import || isImportAllDir(m_inputPath) )
                //        m_operationMode = eOpMode::ImportAll;
                //    else
                //        m_operationMode = eOpMode::ExportAll; //If all else fails, try an export all!
                //}
            }
            else
                throw runtime_error("Cannot determine the desired operation!");
        }
        else
            throw runtime_error("The input path does not exists!");

    }

    int CAudioUtil::Execute()
    {
        int returnval = -1;
        utils::MrChronometer chronoexecuter("Total time elapsed");
        try
        {
            switch(m_operationMode)
            {
                case eOpMode::ExportSWDLBank:
                {
                    cout << "=== Exporting SWD Bank ===\n";
                    returnval = ExportSWDLBank();
                    break;
                }
                case eOpMode::ExportSWDL:
                {
                    cout << "=== Exporting SWD ===\n";
                    returnval = ExportSWDL();
                    break;
                }
                case eOpMode::ExportSMDL:
                {
                    cout << "=== Exporting SMD ===\n";
                    returnval = ExportSMDL();
                    break;
                }
                case eOpMode::ExportSEDL:
                {
                    cout << "=== Exporting SED ===\n";
                    returnval = ExportSEDL();
                    break;
                }
                case eOpMode::BuildSWDL:
                {
                    cout << "=== Building SWD ===\n";
                    returnval = BuildSWDL();
                    break;
                }
                case eOpMode::BuildSMDL:
                {
                    cout << "=== Building SMD ===\n";
                    returnval = BuildSMDL();
                    break;
                }
                case eOpMode::BuildSEDL:
                {
                    cout << "=== Building SED ===\n";
                    returnval = BuildSEDL();
                    break;
                }
                default:
                {
                    throw runtime_error( "Invalid operation mode. Something is wrong with the arguments!" );
                }
            };
        }
        catch(Poco::Exception & e )
        {
            cerr <<"\n" << "<!>- POCO Exception - " <<e.name() <<"(" <<e.code() <<") : " << e.message() <<"\n" <<endl;
        }
        catch( exception &e )
        {
            cerr <<"\n" << "<!>- Exception - " <<e.what() <<"\n" <<"\n";
        }
        return returnval;
    }


//--------------------------------------------
//  Operation
//--------------------------------------------
    int CAudioUtil::ExportSWDLBank()
    {
        return 0;
    }
    
    int CAudioUtil::ExportSWDL()
    {
        return 0;
    }

    int CAudioUtil::ExportSMDL()
    {
        using namespace pmd2::audio;
        Poco::Path inputfile(m_inputPath);
        Poco::Path outputfile;
        string     outfname;

        if( ! m_outputPath.empty() )
            outputfile = Poco::Path(m_outputPath);
        else
            outputfile = inputfile.parent().append( inputfile.getBaseName() ).makeFile();

        outfname = outputfile.getBaseName();

        //Export
        MusicSequence smd = LoadSequence( inputfile.toString() );

        //Write output
        ofstream outstr( outputfile.toString() );
        outstr<<smd.tostr();
        outstr.flush();
        outstr.close();

        ofstream outfile( outputfile.toString() );
        outfile << smd.tostr();
        outfile.close();

        //WriteEventsToMidiFileTest_MF( outputfile.toString(), smd );
        outputfile.setExtension("mid");
        WriteEventsToMidiFileTest( outputfile.toString(), smd );

        //Write meta
        //Write the info that didn't fit in the midi here as XML.

        //Finish

        return 0;
    }

        struct trkremappoint
        {
            unsigned int origtrk     = 0; //The index of the track the events to take are
            unsigned int targettrk   = 0; //The track to move this to

            unsigned int ticksbefore = 0; //Duration before the first event
            unsigned int begindex    = 0; //Index of first event to remap
            unsigned int endindex    = 0; //Index of last event to remap
            unsigned int ticksspan   = 0; //Duration between the first and last event

        };

    /*
        PrepRemapSeq
            Tries to read a track from the position specified to the end. If it finds a program change it stops.
            It returns details on the sequence of event it was able to read before hitting a program change event!
    */
    trkremappoint PrepRemapSeq( const pmd2::audio::MusicTrack & curtrk, size_t pos, uint8_t curprogid, uint32_t & lastpause )
    {
        //static const uint8_t SETPresetCode = static_cast<uint8_t>(DSE::eTrkEventCodes::SetPreset);
        //static const uint8_t DrumProgIdBeg = 0x78;
        //static const uint8_t DrumProgIdEnd = 0x7F;
        trkremappoint rmapseq;
        size_t        evno = pos;
        rmapseq.begindex = pos;

        for( ; evno < curtrk.size(); ++evno )
        {
            DSE::eTrkEventCodes code = static_cast<DSE::eTrkEventCodes>( curtrk[evno].evcode );

            //We got a program change
            if( code == DSE::eTrkEventCodes::SetPreset && curtrk[evno].params.front() != curprogid )
                break;

            //Handle delta-time
            if( curtrk[evno].dt != 0 )
            {
                rmapseq.ticksspan += static_cast<uint8_t>( DSE::TrkDelayCodeVals.at( curtrk[evno].dt ) );
            }

            //Handle pauses
            if( code == DSE::eTrkEventCodes::LongPause )
            {
                lastpause = (static_cast<uint16_t>(curtrk[evno].params.back()) << 8) | curtrk[evno].params.front();
                rmapseq.ticksspan += lastpause;
            }
            else if( code == DSE::eTrkEventCodes::Pause )
            {
                lastpause = curtrk[evno].params.front();
                rmapseq.ticksspan += lastpause;
            }
            else if( code == DSE::eTrkEventCodes::AddToLastPause )
            {
                uint32_t prelastp = lastpause;
                lastpause = prelastp + curtrk[evno].params.front();
                rmapseq.ticksspan += lastpause;
            }
            else if( code == DSE::eTrkEventCodes::RepeatLastPause )
            {
                rmapseq.ticksspan += lastpause;
            }

        }

        rmapseq.endindex = evno;

        return rmapseq;
    }

    /*
        This function reads a music sequence and make a list of sequences of events sharing the same program no per tracks.
        It determines depending on the type of instrument on what MIDI track to put it on!
    */
    vector<vector<trkremappoint>> AnalyzeForRemaps( const pmd2::audio::MusicSequence & seq )
    {
        using pmd2::audio::MusicTrack;
        using pmd2::audio::MusicSequence;
        using namespace DSE;
        vector<MusicTrack>    fixedtracks;

        vector<vector<trkremappoint>> trackremapinfo(seq.getNbTracks());
        //vector<trkremappoint> trackremapinfo;
        //vector<int>           nbprgchangesptrack; //The amount of sub-sequence for each tracks. AKA the nb of program changes.

        //Temporary measure 
        //#TODO: fetch data on each instruments if available, then fallback to other more basic measures !
        static const uint8_t DrumProgIdBeg = 0x78;
        static const uint8_t DrumProgIdEnd = 0x7F;

        for( unsigned int trkno = 0; trkno < seq.getNbTracks(); ++trkno )
        {
            const auto &  curtrk    = seq.track(trkno);
            unsigned int  curprog   = 0; //Current instrument preset
            unsigned int  ticks     = 0;
            uint32_t      lastpause = 0;

            for( size_t evno = 0; evno < curtrk.size(); ++evno )
            {
                const auto & curevent = curtrk[evno];
                const DSE::eTrkEventCodes code = static_cast<DSE::eTrkEventCodes>( curtrk[evno].evcode );

                if( code == eTrkEventCodes::SetPreset )
                {
                    curprog = curevent.params.front();
                    trkremappoint rmap = PrepRemapSeq( curtrk, evno, curprog, lastpause ); //Last pause is passed by reference and modified as needed
                    rmap.origtrk = trkno;

                    //#TODO: replace with function that query the instrument bank!
                    //Decide what to do depending on the instrument
                    if( curprog >= DrumProgIdBeg && curprog <= DrumProgIdEnd ) //If its an instrument from the GM drumkit
                        rmap.targettrk = 9;
                    else if( trkno != 9 ) //If the instrument isn't a drum, and its not on a drum track
                        rmap.targettrk = trkno; //Don't move
                    else //If the instrument is not part of the GM drumkit, but its on MIDI track 10
                    {
                        //#TODO: Need to try to find this sequence a home track with a proper algorithm ! 
                        //For now, merge with track 1
                        rmap.targettrk = 1;
                    }

                    rmap.ticksbefore = ticks;
                    trackremapinfo[trkno].push_back( rmap );

                    ticks += rmap.ticksspan; //increment current ticks accordingly
                    evno  =  rmap.endindex;  //Move cursor to last entry in the sequence
                    //lastpause was already modified as needed
                    continue; //skip the steps below
                }

                //Count DT
                if( curevent.dt != 0 )
                    ticks += static_cast<uint8_t>( DSE::TrkDelayCodeVals.at( curevent.dt ) );

                //Count Pauses
                if( code == DSE::eTrkEventCodes::LongPause )
                {
                    lastpause = (static_cast<uint16_t>(curevent.params.back()) << 8) | curevent.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::Pause )
                {
                    lastpause = curevent.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::AddToLastPause )
                {
                    uint32_t prelastp = lastpause;
                    lastpause = prelastp + curevent.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::RepeatLastPause )
                {
                    ticks += lastpause;
                }
            }
        }

        return std::move( trackremapinfo );
    }


    /*
    */
    uint8_t RemapInstrumentProg( uint8_t progid )
    {
        if( progid == 0x1 ) //new age pad
            return 88;
        else if( progid == 0x2 ) //EPiano 2
            return 5;
        else if( progid == 0x3 ||  progid == 0x4 ) //Synth voice ? Basssoon
            return 70;//102;
        else if( progid == 0x5 ) //xylophone
            return 13;
        else if( progid == 0x6 ) //Vibraphone
            return 11;
        else if( progid == 0x7 || progid == 0x8 || progid == 0x9 /*|| progid == 0x4*/ ) //carillion ? glockenspiel
            return 9;
        else if( progid == 0xA || progid == 0xB ) //Harp
            return 46;
        else if( progid == 0xC ) //Tubular bell
            return 14;
        else if( progid == 0xD ) //Weird indian percussion
            return 116;
        else if( progid == 0xE ) //Harpsichord
            return 6;
        else if( progid == 0xF ) //Crystal ? Fifth ?
            return 76;//86;//98;
        else if( progid == 0x14 || progid == 0x15 ) //Nylon guitar
            return 24;
        else if( progid == 0x16 || progid == 0x17 ) //Dulcimer (15)? Banjo (105)? Koto (107)?
            return 15;
        else if( progid == 0x19 ) //Synth bass1
            return 38;
        else if( progid == 0x1A ) //SynthBass2
            return 39;
        else if( progid == 0x1D ) //Slap bass
            return 36;
        else if( progid == 0x1F || progid == 0x20 ) //Synth Strings?
            return 50;
        else if( progid == 0x23 ) //Choir AH
            return 52;
        else if( progid == 0x27 || progid == 0x28 ) //Slow string
            return 49;
        else if( progid == 0x2A ) //eh? Hard to say, maybe a tenor sax (66) ? English horn (69)?
            return 69;
        else if( progid == 0x2B || progid == 0x2C ) //Percussive organ
            return 17;
        else if( progid == 0x2E ) //String ensemble
            return 48;
        else if( progid == 0x30 ) //Bassoon ?? Probably bagpipes ?
            return 109;
        else if( progid == 0x31 || progid == 0x32 ) //Recorder
            return 74;
        else if( progid == 0x33 || progid == 0x34 ) //Flute
            return 73;
        else if( progid == 0x35 ) //Clarinet
            return 71;
        else if( progid == 0x36 || progid == 0x37 ) //Bassoon(70) ? Maybe Oboe ? English horn ?
            return 69;
        else if( progid == 0x3B || progid == 0x3D ) //Trumpet
            return 56;
        else if( progid == 0x3E ) //Trombone ?
            return 57;
        else if( progid == 0x3F ) //Tuba
            return 58; 
        else if( progid == 0x40 || progid == 0x41 || progid == 0x42 ) //Horn ?
            return 60;
        else if( progid == 0x44 ) //Brass Section ?
            return 61;
        else if( progid == 0x47 || progid == 0x48 ) //Violin
            return 40;//110;
        else if( progid == 0x4A ) //Cello ?
            return 42;
        else if( progid == 0x4B ) //Pizzicato
            return 45;
        else if( progid == 0x51 || progid == 0x52 ) //PanFlute
            return 75;
        else if( progid == 0x53 ) //Steel drums
            return 114;
        else if( progid == 0x54 ) //Sitar
            return 104;
        else if( progid == 0x5B ) //Polysynth
            return 90; 
        else if( progid == 0x5D ) //Synth Brass2
            return 63;
        else if( progid == 0x5E || progid == 0x5F ) //Whistle
            return 78;
        else if( progid == 0x60 ) //Some synth wave?
            return 80;//110;
        else if( progid == 0x61 || progid == 0x62 ) //Sawtooth wave
            return 62;//81;
        else if( progid == 0x63 ) //some kind of synth wave. 
            return 112;
        else if( progid == 0x79 ) //Timpani
            return 47;
        else if( progid == 0x7B ) //Taiko drum ?
            return 116;
        else
            return 1;
    }

    /*
    */
    void WriteEventsToMidiFileTest( const std::string & file, const pmd2::audio::MusicSequence & seq )
    {
        using namespace jdksmidi;
        static const string UtilityID = "ExportedWith:ppmd_audioutil.exe ver0.1";

        //Analyse tracks for remaps
        auto remappings = AnalyzeForRemaps( seq );

        //Build midi file
        MIDIMultiTrack mt( seq.getNbTracks() );
        mt.SetClksPerBeat(48);

        //Init track 0 with time signature
        MIDITimedBigMessage timesig;
        timesig.SetTime( 0 );
        timesig.SetTimeSig();
        mt.GetTrack( 0 )->PutEvent( timesig );
        mt.GetTrack( 0 )->PutTextEvent( 0, META_TRACK_NAME, seq.metadata().fname.c_str(), seq.metadata().fname.size() );
        mt.GetTrack( 0 )->PutTextEvent( 0, META_GENERIC_TEXT, UtilityID.c_str(), UtilityID.size() );

        //Re-assign drumtrack #TODO: It will be important to work out something for tracks set to chan 10 that gets a program change midway !
        vector<uint8_t> midichan  ( seq.getNbTracks(), 0 );
        vector<int>    drumtracks;
        int             wrongdrumtrk = -1;
        for( int i = 0; i < seq.getNbTracks(); ++i )
        {
            midichan[i] = seq.track(i).GetMidiChannel();
            bool Iamdrumtrack = false;
        
            //Find if we're actually a drum track
            for( const auto & ev : seq.track(i) )
            {
                if( ev.evcode == static_cast<uint8_t>(DSE::eTrkEventCodes::SetPreset) && (ev.params.front() == 0x7F || ev.params.front() == 0x7B || ev.params.front() == 0x7E ) )
                {
                    Iamdrumtrack = true;
                    drumtracks.push_back(i);
                    break;
                }
            }

            if( midichan[i] == 9 && !Iamdrumtrack ) //Keep track of who got track 9
                wrongdrumtrk = i;
        }

        //If we can, swap the channel with one of the drumtracks
        if( wrongdrumtrk != -1 )
        {
            if( ! drumtracks.empty() )
            {
                uint8_t swapchan = 0;
                swapchan = midichan[wrongdrumtrk];
                midichan[wrongdrumtrk] = midichan[drumtracks.front()];
                midichan[drumtracks.front()] = swapchan;
                cout <<"!!-- Re-assigned track #" <<wrongdrumtrk <<"'s MIDI channel from 9 to " << static_cast<short>(midichan[wrongdrumtrk]) <<"--!!\n";
            }
            else //We have a track on the drum channel with not actual drum channel in the song
            {
                //Try to find an unused channel
                int chanid = 0;
                for( chanid; chanid < 16; ++chanid )
                {
                    int inusecnt = 0;

                    if( chanid != 9 ) //Skip channel 10 for obvious reasons
                    {
                        for( int i = 0; i < midichan.size(); ++i )
                        {
                            if( midichan[i] == chanid )
                                ++inusecnt;
                        }
                    }

                    if( inusecnt == 0 )
                        break;
                }

                //If we found a free channel, assign it.
                if( chanid != 16 )
                {
                    cout <<"!!-- Re-assigned track #" <<wrongdrumtrk <<"'s MIDI channel from " <<static_cast<short>(midichan[wrongdrumtrk]) <<" to " <<chanid <<"--!!\n";
                    midichan[wrongdrumtrk] = chanid;
                }
                //If we didn't find any, tough luck, can't do much..
            }

        }
        //Set the remaining drumtracks to 10
        for( const auto & drumtrk : drumtracks )
        {
            cout <<"!!-- Re-assigned track #" <<drumtrk <<"'s MIDI channel from " << static_cast<short>(midichan[drumtrk]) <<" to  9 --!!\n";
            midichan[drumtrk] = 9;
        }

        uint32_t lastlasttick = 0; //The nb of ticks of the last track that was handled
        
        for( unsigned int trkno = 0; trkno < seq.getNbTracks(); ++trkno )
        {
            cout<<"Writing track #" <<trkno <<"\n";
            static const uint8_t ForcedProgPiano = 0; //For now force all chromatic instruments to be exported to this one
            uint32_t eventno   = 0; //Event counter to identify a single problematic event
            uint32_t ticks     = 0;
            uint32_t lastpause = 0;
            uint32_t lasthold  = 0; //last duration a note was held
            int8_t   curoctave = 0;
            int8_t   lastoctaveevent = 0;
            uint8_t  currentprog = 0; //Keep track of the current program to apply pitch correction on specific instruments
            bool     sustainon = false; //When a note is sustained, it must be let go of at the next play note event
            uint8_t curchannel = midichan[trkno];

            for( size_t evno = 0; evno < seq.track(trkno).size(); ++evno )// const auto & ev : seq.track(trkno) )
            {
                const auto & ev = seq.track(trkno)[evno];
                MIDITimedBigMessage mess;
                const auto code = static_cast<DSE::eTrkEventCodes>(ev.evcode);

                //Check to what MIDI track we copy the current event
                //unsigned int desttrk = trkno;
                //for( const auto & trktrmaps : remappings[trkno] )
                //{
                //    if( evno >= trktrmaps.begindex && evno >= trktrmaps.endindex )
                //    {
                //        desttrk = trktrmaps.targettrk;
                //        break;
                //    }
                //}

                //Handle delta-time
                if( ev.dt != 0 )
                {
                    if( (ev.dt & 0xF0) != 0x80 )
                        cout << "Bad delta-time ! " <<"( trk#" <<trkno <<", evt #" <<eventno << ")" <<"\n";
                    else
                        ticks += static_cast<uint8_t>( DSE::TrkDelayCodeVals.at( ev.dt ) );
                }

                //Set the time properly now
                mess.SetTime(ticks);

                //Warn if we're writing notes past the ticks of the last track
                //if( lastlasttick!= 0 && ticks > lastlasttick )
                //    cout << "!!-- WARNING: Writing events past the last parsed track's end ! Prev track last ticks : " <<lastlasttick <<", current ticks : " <<ticks <<" --!!" <<"( trk#" <<trkno <<", evt #" <<eventno << ")" <<"\n" ;

                if( trkno == 0 )
                {
                    if( code == DSE::eTrkEventCodes::SetTempo )
                    {
                        //Function convert Tempo BPM to MicSecPerBeat/Quarter Note
                        uint32_t microspquart = 0;
                        {
                            static const uint32_t NbMicrosecPerMinute = 60000000;
                            microspquart= NbMicrosecPerMinute / ev.params.front();
                        }
                        mess.SetTempo( microspquart );
                        mt.GetTrack(trkno)->PutEvent( mess );
                    }
                    continue;
                }

                
                if( code == DSE::eTrkEventCodes::LongPause )
                {
                    lastpause = (static_cast<uint16_t>(ev.params.back()) << 8) | ev.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::Pause )
                {
                    lastpause = ev.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::AddToLastPause )
                {
                    uint32_t prelastp = lastpause;
                    lastpause = prelastp + ev.params.front();
                    ticks += lastpause;
                }
                else if( code == DSE::eTrkEventCodes::RepeatLastPause )
                {
                    ticks += lastpause;
                }
                else if( code >= DSE::eTrkEventCodes::NoteOnBeg && code <= DSE::eTrkEventCodes::NoteOnEnd ) //Handle play note
                {
                    //Turn off sustain if neccessary
                    if( sustainon )
                    {
                        MIDITimedBigMessage susoff;
                        susoff.SetTime(ticks);
                        susoff.SetControlChange( curchannel, 66, 0 ); //sustainato
                        mt.GetTrack(trkno)->PutEvent( susoff );
                        sustainon = false;
                    }


                    if( (ev.params.front() & DSE::NoteEvParam1PitchMask) == static_cast<uint8_t>(DSE::eNotePitch::lower) )
                        --curoctave;
                    else if( (ev.params.front() & DSE::NoteEvParam1PitchMask) == static_cast<uint8_t>(DSE::eNotePitch::higher) )
                        ++curoctave;
                    else if( (ev.params.front() & DSE::NoteEvParam1PitchMask) == static_cast<uint8_t>(DSE::eNotePitch::reset) )
                        curoctave = lastoctaveevent;

                    int8_t notenb  = (ev.params.front() & 0x0F);
                    int8_t mnoteid = notenb + ( (curoctave) * 12 ); //Midi notes begins at -1 octave, while DSE ones at 0..
                    mess.SetTime(ticks);
                    mess.SetNoteOn( curchannel, mnoteid, static_cast<uint8_t>(ev.evcode & 0x7F) );
                    mt.GetTrack(trkno)->PutEvent( mess );
                     
                    if( ev.params.size() >= 2 )
                    {
                        if( ev.params.size() == 2 )
                            lasthold = ev.params[1];
                        else if( ev.params.size() == 3 )
                            lasthold = static_cast<uint16_t>( ev.params[1] << 8 ) | ev.params[2];
                        else if( ev.params.size() == 4 )
                        {
                            lasthold = static_cast<uint32_t>( ev.params[1] << 16 ) | ( ev.params[2] << 8 ) | ev.params[3];
                            cout<<"##Got Note Event with 3 bytes long hold! Parsed as " <<lasthold <<"!##" <<"( trk#" <<trkno <<", evt #" <<eventno << ")" <<"\n";
                        }
                    }
                    MIDITimedBigMessage noteoff;
                    noteoff.SetTime( ticks + lasthold );
                    noteoff.SetNoteOff( curchannel, mnoteid, static_cast<uint8_t>(ev.evcode & 0x7F) ); //Set proper channel from original track eventually !!!!

                    mt.GetTrack(trkno)->PutEvent( noteoff );
                }
                else if( code == DSE::eTrkEventCodes::SetOctave )
                {
                    lastoctaveevent = ev.params.front(); 

                    //#TOOD: implement some kind of proper pitch correction lookup table..
                    // Should be done only when exporting to GM ! Or else the sample's pitch won't match anymore..
                    if( currentprog == 0x19 || currentprog == 0x1A || currentprog == 0x1D || currentprog == 0x33 || 
                        currentprog == 0x34 || currentprog == 0x47 || currentprog == 0x48 || currentprog == 0x40 || 
                        currentprog == 0x41 || currentprog == 0x42 || currentprog == 0xA  || currentprog == 0xB  ||
                        currentprog == 0x79)
                    {
                        cout <<"Correcting instrument pitch from " <<static_cast<uint16_t>(lastoctaveevent) <<" to " <<static_cast<uint16_t>(lastoctaveevent-1) <<"..\n";
                        --lastoctaveevent;
                    }
                    
                    curoctave = lastoctaveevent;
                }
                else if( code == DSE::eTrkEventCodes::SetExpress )
                {
                    mess.SetControlChange( curchannel, 0x0B, ev.params.front() );
                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::SetTrkVol )
                {
                    mess.SetControlChange( curchannel, 0x07, ev.params.front() );
                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::SetTrkPan )
                {
                    mess.SetControlChange( curchannel, 0x0A, ev.params.front() );
                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::SetPreset )
                {
                    //Leave a note with the original DSE instrument ID 
                    stringstream sstr;
                    sstr << "DSEProg(0x" <<hex <<uppercase <<static_cast<uint16_t>(ev.params.front()) <<dec <<nouppercase <<")";
                    string dseprog = sstr.str();
                    mt.GetTrack(trkno)->PutTextEvent( ticks, META_GENERIC_TEXT, dseprog.c_str(), dseprog.size() );

                    //Keep track of the current program to apply pitch correction on instruments that need it..
                    currentprog = ev.params.front();

                    //#TODO: Handle drum preset changes better
                    //if( ev.params.front() < 0x7E )
                    //    mess.SetProgramChange( curchannel, RemapInstrumentProg(ev.params.front()) ); //NOT DRUMS
                    //else
                        mess.SetProgramChange( curchannel, 0 ); //DRUMS

                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::Modulate )
                {
                    uint16_t mod  = ( static_cast<uint16_t>( (ev.params.front() & 0xF) << 8 ) | ev.params.back());
                    uint16_t FtoCmod = ((mod * 127) / 0x3FFF); //Convert fine mod to coarse, knowing the max for fine is 0x3FFF. finemod / 0x3FFF -> coarsemod/127
                    cout <<"CC#1 Modwheel : " <<FtoCmod <<"\n";

                    //MIDITimedBigMessage portatoggle;
                    //portatoggle.SetTime(ticks);

                    //if( ev.params.front() != 0 && ev.params.back() != 0 )
                    //    portatoggle.SetControlChange( curchannel, 65, 64 ); //Toggle portamento on (64 == on)
                    //else
                    //    portatoggle.SetControlChange( curchannel, 65, 63 ); //Toggle portamento off (63 == off)
                    //mt.GetTrack(trkno)->PutEvent( portatoggle );

                    //Fine Portatime 0x25
                    
                   // mess.SetControlChange( curchannel, 0x25,  ev.params.back() );
                    //mess.SetByte5( ev.params.front() );


                    //mess.SetPitchBend( seq.track(trkno).GetMidiChannel(), pitchw );

                    mess.SetControlChange( curchannel, 1, FtoCmod /*ev.params.back()*/ ); //Coarse Modulation Wheel
                    mess.SetByte5( ev.params.front() );
                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::HoldNote )
                {
                    cout<<"Got hold note event ! Trying a sustainato!\n";
                    //Put text here to mark the loop point for software that can't handle loop events
                    static const std::string TXT_HoldNote = "HoldNote";
                    mt.GetTrack(trkno)->PutTextEvent( ticks, META_GENERIC_TEXT, TXT_HoldNote.c_str(), TXT_HoldNote.size() );

                    //Put a sustenato
                    sustainon = true;
                    mess.SetControlChange( curchannel, 66, 127 ); //sustainato
                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::LoopPointSet )
                {
                    //mess.SetMetaEvent( META_TRACK_LOOP, 0 );
                    mess.SetMetaType(META_TRACK_LOOP);
                    
                    //Put text here to mark the loop point for software that can't handle loop events
                    static const std::string TXT_Loop = "LoopStart";
                    mt.GetTrack(trkno)->PutTextEvent( ticks, META_MARKER_TEXT, TXT_Loop.c_str(), TXT_Loop.size() );

                    mt.GetTrack(trkno)->PutEvent( mess );
                }
                else if( code == DSE::eTrkEventCodes::Unk_0xBE )
                {
                    //uint16_t mod  = ( static_cast<uint16_t>( (ev.params.front() & 0xF) << 8 ) | ev.params.back());
                    //uint16_t FtoCmod = ((mod * 127) / 0x3FFF); //Convert fine mod to coarse, knowing the max for fine is 0x3FFF. finemod / 0x3FFF -> coarsemod/127
                    //cout <<"CC# Modwheel : " <<FtoCmod <<"\n";

                    //mess.SetControlChange( curchannel, 1, FtoCmod /*ev.params.back()*/ ); //Coarse Modulation Wheel
                    //mess.SetByte5( ev.params.front() );
                    //mt.GetTrack(trkno)->PutEvent( mess );
                }
                //else if( code == DSE::eTrkEventCodes::SetUnk1 )
                //{
                //    cout<<"Got SetUnk1 event ! Marking and skipping..\n";
                //    stringstream sstr;
                //    sstr << "SetUnk1(" <<ev.evcode <<ev.params.front() <<")"; //Store as raw bytes
                //    string setunk1 = sstr.str();
                //    mt.GetTrack(trkno)->PutTextEvent( ticks, META_GENERIC_TEXT, setunk1.c_str(), setunk1.size() );
                //}
                //else if( code == DSE::eTrkEventCodes::SetUnk2 )
                //{
                //    cout<<"Got SetUnk2 event ! Marking and skipping..\n";
                //    stringstream sstr;
                //    sstr << "SetUnk2(" <<ev.evcode <<ev.params.front() <<")"; //Store as raw bytes
                //    string setunk2 = sstr.str();
                //    mt.GetTrack(trkno)->PutTextEvent( ticks, META_GENERIC_TEXT, setunk2.c_str(), setunk2.size() );
                //}
                else if( code != DSE::eTrkEventCodes::EndOfTrack )
                {
                    stringstream sstr;
                    sstr << "UNK(" <<ev.evcode;
                    cout<<"Got event 0x" <<uppercase <<hex <<static_cast<uint16_t>(code) <<dec <<nouppercase <<" ! With";

                    for( const auto & param : ev.params )
                    {
                        cout <<" 0x" <<hex <<uppercase <<static_cast<uint16_t>(param) <<dec <<nouppercase;
                        sstr << param; //Store as raw bytes
                    }
                    cout <<" !\n";
                    sstr <<")";
                    string unkev = sstr.str();
                    mt.GetTrack(trkno)->PutTextEvent( ticks, META_GENERIC_TEXT, unkev.c_str(), unkev.size() );
                }

                //Event done increment event counter
                ++eventno;
            }

            //Track is done, save last tick value
            lastlasttick = ticks;
        }

        //After all done
        mt.SortEventsOrder();

        MIDIFileWriteStreamFileName out_stream( file.c_str() );

        // then output the stream like my example does, except setting num_tracks to match your data

        if( out_stream.IsValid() )
        {
            // the object which takes the midi tracks and writes the midifile to the output stream
            MIDIFileWriteMultiTrack writer( &mt, &out_stream );

            // write the output file
            if ( writer.Write( mt.GetNumTracks() ) )
            {
                cout << "\nOK writing file " << file << endl;
            }
            else
            {
                cerr << "\nError writing file " << file << endl;
            }
        }
        else
        {
            cerr << "\nError opening file " << file << endl;
        }
    }

    enum struct eMidiMessCodes : uint8_t
    {
        NoteOff     = 0x80,
        NoteOn      = 0x90,
        Aftertouch  = 0xA0,
        CtrlChange  = 0xB0,
        PrgmChange  = 0xC0,
        ChanPress   = 0xD0,
        PitchWheel  = 0xE0,
        SySexcl     = 0xF0,
    };

    //midifile::MidiEvent ToMidiEvent( DSE::TrkEvent ev )
    //{
    //    const auto code = static_cast<DSE::eTrkEventCodes>( ev.evcode );
    //    if( code >= DSE::eTrkEventCodes::NoteOnBeg && code <= DSE::eTrkEventCodes::NoteOnEnd )
    //    {

    //    }
    //    else
    //    {
    //        switch( code )
    //        {
    //            case DSE::eTrkEventCodes::SetTempo:
    //            {
    //                break;
    //            }
    //            case DSE::eTrkEventCodes::SetExpress:
    //            {
    //                break;
    //            }
    //            case DSE::eTrkEventCodes::SetTrkVol:
    //            {
    //                break;
    //            }
    //            case DSE::eTrkEventCodes::SetTrkPan:
    //            {
    //                break;
    //            }
    //            case DSE::eTrkEventCodes::SetPreset:
    //            {
    //                break;
    //            }
    //            case DSE::eTrkEventCodes::Modulate:
    //            {
    //                break;
    //            }
    //        };
    //    }

    //    return ;
    //}

    //void WriteEventsToMidiFileTest_MF( const std::string & file, const pmd2::audio::MusicSequence & seq )
    //{
    //    //using namespace midifile;
    //    MidiFile mf;
    //    
    //    mf.absoluteTicks();
    //    mf.addTrack( (seq.getNbTracks()-1) );
    //    mf.setTicksPerQuarterNote( 48 );

    //    //Add time sig event on track 0
    //    mf.addEvent( 0, 0, vector<uint8_t>{ 0xFF, 0x58, 4, 4, 2, 24, 8 } );

    //    for( unsigned int trkno = 0; trkno < seq.getNbTracks(); ++trkno )
    //    {
    //        cout<<"Writing track #" <<trkno <<"\n";
    //        uint32_t ticks     = 0;
    //        uint32_t lastpause = 0;
    //        uint8_t  curoctave = 0;

    //        for( const auto & ev : seq.track(trkno) )
    //        {
    //            const auto code = static_cast<DSE::eTrkEventCodes>(ev.evcode);

    //            //Handle pauses if neccessary
    //            if( code == DSE::eTrkEventCodes::LongPause )
    //            {
    //                lastpause = (static_cast<uint16_t>(ev.params.back()) << 8) | ev.params.front();
    //                ticks += lastpause;
    //            }
    //            else if( code == DSE::eTrkEventCodes::Pause )
    //            {
    //                lastpause = ev.params.front();
    //                ticks += lastpause;
    //            }
    //            else if( code == DSE::eTrkEventCodes::AddToLastPause )
    //            {
    //                uint32_t prelastp = lastpause;
    //                lastpause = prelastp + ev.params.front();
    //                ticks += lastpause;
    //            }
    //            else if( code == DSE::eTrkEventCodes::RepeatLastPause )
    //            {
    //                ticks += lastpause;
    //            }

    //            //Handle delta-time
    //            if( ev.dt != 0 )
    //                ticks += static_cast<uint8_t>( DSE::TrkDelayCodeVals.at( ev.dt ) );


    //            if( trkno == 0 )
    //            {
    //                //if( code == DSE::eTrkEventCodes::SetTempo )
    //                //{
    //                //    static const uint32_t NbMicrosecPerMinute = 60000000;
    //                //    uint32_t microspquart= NbMicrosecPerMinute / ev.params.front();
    //                //    uint8_t asbytes[3] = { static_cast<uint8_t>(microspquart >> 16), static_cast<uint8_t>(microspquart >> 8), static_cast<uint8_t>(microspquart) };
    //                //    mf.addMetaEvent( trkno, ticks, 0x51, vector<uint8_t>{0x03, asbytes[0], asbytes[1], asbytes[2] } );
    //                //}
    //                continue;
    //            }

    //            //Handle play note
    //            if( code >= DSE::eTrkEventCodes::NoteOnBeg && code <= DSE::eTrkEventCodes::NoteOnEnd )
    //            {
    //                //MidiEvent mev;
    //                uint8_t   mnoteid = ev.params.front() & 0x0F + ( (curoctave+1) * 12 ); //Midi notes begins at -1 octave, while DSE ones at 0..
    //                //mev.makeNoteOn( mnoteid, ev.evcode, trkno );
    //                mf.addEvent( trkno, ticks, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::NoteOn), static_cast<uint8_t>(mnoteid & 0x7F), static_cast<uint8_t>(ev.evcode & 0x7F) } );
    //                //MidiEvent mevoff;
    //                //mevoff.makeNoteOff( mnoteid, ev.evcode, trkno );
    //                 
    //                uint8_t notehold = 2; //By default hold for the shortest note duration
    //                if( ev.params.size() >= 2 )
    //                {
    //                    notehold = ev.params[1];
    //                }

    //                mf.addEvent( trkno, ticks + notehold, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::NoteOff), static_cast<uint8_t>(mnoteid & 0x7F), static_cast<uint8_t>(ev.evcode & 0x7F) } );
    //            }
    //            else if( code == DSE::eTrkEventCodes::SetOctave )
    //            {
    //                curoctave = ev.params.front();
    //            }
    //            else if( code == DSE::eTrkEventCodes::SetExpress )
    //            {
    //                mf.addEvent( trkno, ticks, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::CtrlChange), 0x0B, ev.params.front() } );
    //            }
    //            else if( code == DSE::eTrkEventCodes::SetTrkVol )
    //            {
    //                mf.addEvent( trkno, ticks, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::CtrlChange), 0x07, ev.params.front() } );
    //            }
    //            else if( code == DSE::eTrkEventCodes::SetTrkPan )
    //            {
    //                mf.addEvent( trkno, ticks, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::CtrlChange), 0x0A, ev.params.front() } );
    //            }
    //            else if( code == DSE::eTrkEventCodes::SetPreset )
    //            {
    //                mf.addEvent( trkno, ticks, vector<uint8_t>{ static_cast<uint8_t>(eMidiMessCodes::PrgmChange), ev.params.front() } );
    //            }
    //        }
    //    }

    //    //FF 2F 00  == End of track

    //    mf.sortTracks();
    //    mf.deltaTicks();

    //    stringstream sstr;
    //    sstr <<file <<".mid";
    //    mf.write( sstr.str() );
    //}

    //void WriteMusicDump( const pmd2::audio::MusicSequence & seq, const std::string & fname )
    //{

    //}

    int CAudioUtil::ExportSEDL()
    {
        return 0;
    }

    int CAudioUtil::BuildSWDL()
    {
        return 0;
    }

    int CAudioUtil::BuildSMDL()
    {
        return 0;
    }

    int CAudioUtil::BuildSEDL()
    {
        return 0;
    }

//--------------------------------------------
//  Main Methods
//--------------------------------------------
    int CAudioUtil::Main(int argc, const char * argv[])
    {
        int returnval = -1;
        PrintTitle();

        //Handle arguments
        returnval = GatherArgs( argc, argv );
        if( returnval != 0 )
            return returnval;
        
        //Execute the utility
        returnval = Execute();

#ifdef _DEBUG
        system("pause");
#endif

        return returnval;
    }
};

//=================================================================================================
// Main Function
//=================================================================================================

//#TODO: Move the main function somewhere else !
int main( int argc, const char * argv[] )
{
    using namespace audioutil;
    try
    {
        CAudioUtil & application = CAudioUtil::GetInstance();
        return application.Main(argc,argv);
    }
    catch( exception & e )
    {
        cout<< "<!>-ERROR:" <<e.what()<<"\n"
            << "If you get this particular error output, it means an exception got through, and the programmer should be notified!\n";
    }

    return 0;
}