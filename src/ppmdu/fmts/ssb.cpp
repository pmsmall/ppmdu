#include "ssb.hpp"
#include <ppmdu/pmd2/pmd2_scripts.hpp>
#include <ppmdu/pmd2/pmd2_scripts_opcodes.hpp>
#include <utils/utility.hpp>
#include <iostream>
#include <sstream>

using namespace pmd2;
using namespace std;



namespace filetypes
{
    /*
        group_entry
            Script instruction group entry
    */
    struct group_entry
    {
        static const size_t LEN = 3 * sizeof(uint16_t);
        uint16_t begoffset = 0;
        uint16_t type      = 0;
        uint16_t unk2      = 0;

        template<class _outit>
            _outit WriteToContainer(_outit itwriteto)const
        {
            itwriteto = utils::WriteIntToBytes(begoffset,  itwriteto);
            itwriteto = utils::WriteIntToBytes(type,       itwriteto);
            itwriteto = utils::WriteIntToBytes(unk2,       itwriteto);
            return itwriteto;
        }

        //
        template<class _init>
            _init ReadFromContainer(_init itReadfrom, _init itpastend)
        {
            itReadfrom = utils::ReadIntFromBytes(begoffset, itReadfrom, itpastend );
            itReadfrom = utils::ReadIntFromBytes(type,      itReadfrom, itpastend );
            itReadfrom = utils::ReadIntFromBytes(unk2,      itReadfrom, itpastend );
            return itReadfrom;
        }
    };

//=======================================================================================
//  SSB Parser
//=======================================================================================

    template<eOpCodeVersion>
        struct OpCodeFinderPicker;

    template<>
        struct OpCodeFinderPicker<eOpCodeVersion::EoS>
    {
        inline const OpCodeInfoEoS * operator()( uint16_t opcode )
        {
            return FindOpCodeInfo_EoS(opcode);
        }
    };

    template<>
        struct OpCodeFinderPicker<eOpCodeVersion::EoTD>
    {
        inline const OpCodeInfoEoTD * operator()( uint16_t opcode )
        {
            return FindOpCodeInfo_EoTD(opcode);
        }
    };


    /*
    */
    template<typename _Init>
        class SSB_Parser
    {

    public:
        typedef _Init                           initer;

        SSB_Parser( _Init beg, _Init end, eOpCodeVersion scrver, eGameLocale scrloc )
            :m_beg(beg), m_end(end), m_cur(beg), m_scrversion(scrver), m_scrlocale(scrloc)
        {}

        ScriptedSequence Parse()
        {
            m_out = std::move(ScriptedSequence());
            ParseHeader   ();
            ParseGroups   ();
            ParseCode     ();
            ParseConstants();
            return std::move(m_out);
        }

    private:

        void ParseHeader()
        {
            uint16_t scriptdatalen = 0;

            if( m_scrlocale == eGameLocale::NorthAmerica )
            {
                ssb_header hdr;
                m_hdrlen = ssb_header::LEN;
                m_cur = hdr.ReadFromContainer( m_cur, m_end );

                m_nbconsts    = hdr.nbconst;
                m_nbstrs      = hdr.nbstrs;
                scriptdatalen = hdr.scriptdatlen;
                m_stringblksSizes.push_back( hdr.strtbllen * ScriptWordLen );
            }
            else if( m_scrlocale == eGameLocale::Europe )
            {
                ssb_header_pal hdr;
                m_hdrlen = ssb_header_pal::LEN;
                m_cur = hdr.ReadFromContainer( m_cur, m_end );

                m_nbconsts    = hdr.nbconst;
                m_nbstrs      = hdr.nbstrs;
                scriptdatalen = hdr.scriptdatlen;
                m_stringblksSizes.push_back( hdr.strenglen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strfrelen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strgerlen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.stritalen * ScriptWordLen );
                m_stringblksSizes.push_back( hdr.strspalen * ScriptWordLen );
            }
            else if( m_scrlocale == eGameLocale::Japan )
            {
                cout<<"SSB_Parser::ParseHeader(): Japanese handling not implemented yet!\n";
                assert(false);
            }
            else
            {
                cout<<"SSB_Parser::ParseHeader(): Unknown script locale!!\n";
                assert(false);
            }

            //Parse SSB Header
            
            //Parse Data header
            m_cur = m_dathdr.ReadFromContainer( m_cur, m_end );

            //Compute offsets
            m_datalen        = (m_dathdr.datalen * ScriptWordLen);
            m_constoffset    = m_hdrlen + m_datalen;         //Group table is included into the datalen
            //m_codeoffset    = header_t::LEN + ssb_data_hdr::LEN + (m_dathdr.nbgrps * group_entry::LEN);
            m_stringblockbeg = m_hdrlen + (scriptdatalen * ScriptWordLen);
        }

        void ParseGroups()
        {
            m_grps.resize( m_dathdr.nbgrps );
            //Grab all groups
            for( size_t cntgrp = 0; cntgrp < m_dathdr.nbgrps; ++cntgrp )
            {
                m_cur = m_grps[cntgrp].ReadFromContainer( m_cur, m_end );
            }

            //!#TODO: Do some validation I guess?
        }

        void ParseCode()
        {
            if( m_scrversion == eOpCodeVersion::EoS ) 
                ParseCodeWithOpCodeFinder(OpCodeFinderPicker<eOpCodeVersion::EoS>());
            else if( m_scrversion == eOpCodeVersion::EoTD )
                ParseCodeWithOpCodeFinder(OpCodeFinderPicker<eOpCodeVersion::EoTD>());
            else
            {
                clog << "\n<!>- SSB_Parser::ParseCode() : INVALID SCRIPT VERSION!!\n";
                assert(false);
            }
        }

        template<typename _InstFinder>
            void ParseCodeWithOpCodeFinder(_InstFinder & opcodefinder)
        {
            //Iterate through all group and grab their instructions.
            for( size_t cntgrp = 0; cntgrp < m_dathdr.nbgrps; ++cntgrp )
            {
                ScriptInstrGrp grp;
                grp.instructions = std::move( ParseInstructionSequence( (m_grps[cntgrp].begoffset * ScriptWordLen) + m_hdrlen, opcodefinder ) );
                grp.type         = m_grps[cntgrp].type;
                grp.unk2         = m_grps[cntgrp].unk2;
                m_out.Groups().push_back( std::move(grp) );
            }
        }


        template<typename _InstFinder>
            deque<ScriptInstruction> ParseInstructionSequence( size_t foffset, _InstFinder & opcodefinder )
        {
            deque<ScriptInstruction> sequence;
            m_cur = m_beg;
            std::advance( m_cur, foffset ); 


            while( m_cur != m_end )
            {
                uint16_t curop = utils::ReadIntFromBytes<uint16_t>( m_cur, m_end );

                if( curop != NullOpCode )
                {
                    auto opcodedata = opcodefinder(curop);
                    
                    if( opcodedata == nullptr )
                    {
#ifdef _DEBUG
                        assert(false);
#endif
                        throw std::runtime_error("SSB_Parser::ParseInstructionSequence() : Unknown Opcode!");
                    }

                    ScriptInstruction inst;
                    inst.opcode = curop;
                    
                    if( opcodedata->nbparams >= 0 )
                    {
                        const uint16_t nbparams = static_cast<uint16_t>(opcodedata->nbparams);
                        for( size_t cntparam = 0; cntparam < nbparams; ++cntparam )
                            inst.parameters.push_back( utils::ReadIntFromBytes<uint16_t>(m_cur, m_end) );
                        sequence.push_back(std::move(inst));
                    }
                    else
                    {
                        cerr << "\n<!>- Found instruction with -1 parameter number in this script!\n";
                        clog << "\n<!>- Found instruction with -1 parameter number in this script!\n";
                        assert(false);
                    }

                }
                else
                    break;
            }

        }


        void ParseConstants()
        {
            m_out.ConstTbl() = std::move(ParseOffsetTblAndStrings( m_constoffset,  m_nbconsts ));
        }

        void ParseStrings()
        {
            //Parse the strings for any languages we have
            size_t strparseoffset = m_stringblockbeg;
            for( size_t i = 0; i < m_stringblksSizes.size(); ++i )
            {
                m_out.StrTbl(i) = std::move(ParseOffsetTblAndStrings( strparseoffset, m_nbstrs ));
                strparseoffset += m_stringblksSizes[i]; //Add the size of the last block, so we have the offset of the next table
            }
        }

        std::deque<std::string> ParseOffsetTblAndStrings( size_t foffset, uint16_t nbtoparse )
        {
            std::deque<std::string> strings;
            //Parse regular strings here
            initer itstrtbl = m_beg;
            std::advance( itstrtbl, foffset );
            initer itcurtable = itstrtbl;

            //Parse string table
            for( size_t cntstr = 0; cntstr < nbtoparse && itcurtable != m_end; ++cntstr )
            {
                uint16_t stroffset = utils::ReadIntFromBytes<uint16_t>( itcurtable, m_end ); //Offset is in bytes this time!
                initer   itstr     = itstrtbl;
                std::advance(itstr,stroffset);
                strings.push_back( std::move(utils::ReadCStrFromBytes( itstr, m_end )) );
            }
            return std::move(strings);
        }

    private:
        initer              m_beg;
        initer              m_cur;
        initer              m_end;
        ScriptedSequence    m_out;

        size_t              m_hdrlen;
        ssb_data_hdr        m_dathdr;

        uint16_t            m_nbstrs;
        uint16_t            m_nbconsts;
        vector<uint16_t>    m_stringblksSizes;     //in bytes //The lenghts of all strings blocks for each languages

        size_t              m_datalen;             //in bytes //Length of the Data block in bytes
        size_t              m_constoffset;         //in bytes //Start of the constant block
        size_t              m_stringblockbeg;      //in bytes //Start of strings blocks

        vector<group_entry> m_grps;

        eOpCodeVersion       m_scrversion; 
        eGameLocale        m_scrlocale;
    };

//=======================================================================================
//  SSB Writer
//=======================================================================================

//=======================================================================================
//  Functions
//=======================================================================================
    /*
        ParseScript
    */
    pmd2::ScriptedSequence ParseScript(const std::string & scriptfile, eGameLocale gloc, eGameVersion gvers)
    {
        vector<uint8_t> fdata( std::move(utils::io::ReadFileToByteVector(scriptfile)) );
        eOpCodeVersion opvers = eOpCodeVersion::EoS;

        if( gvers == eGameVersion::EoS )
            opvers = eOpCodeVersion::EoS;
        else if( gvers == eGameVersion::EoTEoD )
            opvers = eOpCodeVersion::EoTD;
        else
            throw std::runtime_error("ParseScript(): Wrong game version!!");

        return std::move( SSB_Parser<vector<uint8_t>::const_iterator>(fdata.begin(), fdata.end(), opvers, gloc).Parse() );
    }

    /*
        WriteScript
    */
    void WriteScript( const std::string & scriptfile, const pmd2::ScriptedSequence & scrdat )
    {
        assert(false);
    }


};