#ifndef DSE_INTERPRETER_HPP
#define DSE_INTERPRETER_HPP
/*
dse_interpreter.hpp
2015/07/01
psycommando@gmail.com
Description: This class is meant to interpret a sequence of DSE audio events into standard MIDI events or text.

License: Creative Common 0 ( Public Domain ) https://creativecommons.org/publicdomain/zero/1.0/
All wrongs reversed, no crappyrights :P
*/
#include <ppmdu/pmd2/pmd2_audio_data.hpp>
#include <dse/dse_common.hpp>
#include <dse/dse_sequence.hpp>
#include <vector>
#include <map>
#include <cstdint>

namespace DSE
{
    //#TODO: Put this into a MIDI utility header, or something like that..
    inline uint32_t ConvertTempoToMicrosecPerQuarterNote( uint32_t bpm )
    {
        static const uint32_t NbMicrosecPerMinute = 60000000;
        return NbMicrosecPerMinute / bpm;
    }


    //Remapping data for a single sequence
    //struct PresetBankDB
    //{
    //    typedef uint16_t dsepresid_t;

    //    struct RemapData
    //    {
    //        uint16_t sf2bank;
    //        uint16_t sf2preset;
    //    };

    //    inline void AddRemapPreset( dsepresid_t dseid, RemapData && remapdat )
    //    {
    //        presetstbl.emplace( std::move( std::make_pair( dseid, std::move(remapdat) ) ) );
    //    }

    //    std::map<dsepresid_t, RemapData> presetstbl;
    //};


    /*
        This is used to convert dse tracks into midi.
        It contains details on how to remap notes, what midi preset and bank to use for certain DSE Presets.

        #TODO: Setup the interpreter to use this as one of its parameter. Move implementation to cpp.
    */
    struct SMDLPresetConversionInfo
    {
        typedef uint16_t dsepresetid_t;
        typedef uint8_t  bankid_t;
        typedef uint8_t  presetid_t;
        typedef int8_t   midinote_t;

        struct PresetConvData
        {
            presetid_t                       _midipres;   //The midi preset to use for this preset
            bankid_t                         _midibank;   //The midi bank to use for this preset
            std::map<midinote_t, midinote_t> _remapnotes; //Data on how to remap notes for instruments with complex splits, like drum presets
        };

        //
        std::pair<presetid_t,bankid_t> GetPresetAndBank( dsepresetid_t dsep )
        {
            if( !_convtbl.empty() )
            {
                auto itfound = _convtbl.find(dsep);
                if( itfound != _convtbl.end() )
                    return std::move( std::make_pair(itfound->second._midipres, itfound->second._midibank) );
            }
            return std::move( std::make_pair( static_cast<presetid_t>(dsep), static_cast<bankid_t>(0) ) );
        }

        //
        midinote_t RemapNote( dsepresetid_t dsep, midinote_t note )
        {
            if( !_convtbl.empty() )
            {
                auto itfound = _convtbl.find(dsep);

                if( itfound != _convtbl.end() )
                {
                    auto foundnote = itfound->second._remapnotes.find(note);
                    if( foundnote != itfound->second._remapnotes.end() )
                        return foundnote->second;
                }
            }

            return note;
        }

        inline void AddRemapPreset( dsepresetid_t dseid, PresetConvData && remapdat )
        {
            _convtbl.emplace( std::make_pair( dseid, std::move(remapdat) ) );
        }

        //Instruments conversion table
        std::map<dsepresetid_t, PresetConvData> _convtbl;
    };


    /*
        eMIDIFormat
            The standard MIDI file format to use to export the MIDI data.
            - SingleTrack : Is format 0, a single track for all events.
            - MultiTrack  : Is format 1, one dedicated tempo track, and all the other tracks for events.
    */
    enum struct eMIDIFormat
    {
        SingleTrack,
        MultiTrack,
    };


    /*
        eMIDIMode
            The MIDI file's "sub-standard".
            - GS inserts a GS Mode reset SysEx event, and then turns the drum channel off.
            - XG insets a XG reset Sysex event.
            - GM doesn't insert any special SysEx events.
    */
    enum struct eMIDIMode
    {
        GM,
        GS,
        XG,
    };

    /*
        -presetbanks : The list for each presets of the bank to use
    */
    //void SequenceToMidi( const std::string                 & outmidi, 
    //                     const MusicSequence               & seq, 
    //                     const std::map<uint16_t,uint16_t> & presetbanks,
    //                     eMIDIFormat                         midfmt      = eMIDIFormat::SingleTrack,
    //                     eMIDIMode                           midmode     = eMIDIMode::GS );

    void SequenceToMidi( const std::string              & outmidi, 
                         const MusicSequence            & seq, 
                         const SMDLPresetConversionInfo & remapdata,
                         eMIDIFormat                      midfmt      = eMIDIFormat::SingleTrack,
                         eMIDIMode                        midmode     = eMIDIMode::GS );

    /*
        Exports the track, and make sure as much as possible that playback on GM
        compatible devices and software is optimal. 

        It uses XG or GS to switch track states for drum tracks, and forces single track midi mode.

        -presetconv : Each indices in this vector represents a preset used in the source music sequence.
                      Each entries at those indices indicates what preset to use instead when exporting the
                      MIDI file.
    */
    void SequenceToGM_MIDI( const std::string          & outmidi, 
                            const MusicSequence        & seq, 
                            const std::vector<uint8_t> & presetconv,
                            eMIDIMode                    midmode     = eMIDIMode::GS );

    /****************************************************************************************
        IRenderEngine
            Base class for rendering engines available to render a sequence.

            _OutputTy  : The output object to use for outputing the result into 
                          the output stream.
                
            _HandlerTy : The object handling the data format in the sequence to render, 
                          and feeding that to the _OutputTy object.
    ****************************************************************************************/
    //template<class _OutputTy, class _HandlerTy>
    //    class IRenderEngine
    //{
    //public:
    //    virtual ~IRenderEngine() = 0;

    //    //If is true, the engine renders the sequence over time. If false, it renders it instantaneously.
    //    virtual bool isRealTime()const = 0;

    //    //
    //    virtual bool isRendering()const = 0;

    //    //
    //    virtual void Render( typename _OutputTy::outstrm_t & dest ) = 0;

    //    //
    //    virtual void              setOutput( _OutputTy & out ) = 0;
    //    virtual _OutputTy       & getOutput()                  = 0;
    //    virtual const _OutputTy & getOutput()const             = 0;

    //    //
    //    virtual void               setHandler( _HandlerTy & hnlder ) = 0;
    //    virtual _HandlerTy       & getHandler()                      = 0;
    //    virtual const _HandlerTy & getHandler()const                 = 0;
    //};

    ///****************************************************************************************
    //    BaseRenderOutput
    //        Base class for objects that provide a mean of outputing data in a certain 
    //         specific format, using a specific format of data.
    //****************************************************************************************/
    //template<class _MessageTy, class _TimeTy = tick_t>
    //    class BaseRenderOutput
    //{
    //public:
    //    virtual ~BaseRenderOutput()=0;

    //    virtual void Present( _TimeTy ticks, _MessageTy & mess ) = 0;
    //};

    /*
        DSE_Renderer
            Wraper class around the rendering of DSE sequences to other forms.
    */
    //class DSE_Renderer 
    //{
    //public:
    //    DSE_Renderer();

    //    void Render( const pmd2::audio::MusicSequence & seq, std::ostream & out );

    //private:

    //};

//===============================================================================
//  Handler Functions
//===============================================================================


};

#endif