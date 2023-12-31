/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

inline double doLinearInterpolation(double y1, double y2, double fractional_X)
{
	// --- check invalid condition
	if (fractional_X >= 1.0) return y2;

	// --- use weighted sum method of interpolating
	return fractional_X*y2 + (1.0 - fractional_X)*y1;
}


template <typename T>
class CircularBuffer
{
public:
	CircularBuffer() {}		/* C-TOR */
	~CircularBuffer() {}	/* D-TOR */

							/** flush buffer by resetting all values to 0.0 */
	void flushBuffer(){ memset(&buffer[0], 0, bufferLength * sizeof(T)); }

	/** Create a buffer based on a target maximum in SAMPLES
	//	   do NOT call from realtime audio thread; do this prior to any processing */
	void createCircularBuffer(unsigned int _bufferLength)
	{
		// --- find nearest power of 2 for buffer, and create
		createCircularBufferPowerOfTwo((unsigned int)(pow(2, ceil(log(_bufferLength) / log(2)))));
	}

	/** Create a buffer based on a target maximum in SAMPLESwhere the size is
	    pre-calculated as a power of two */
	void createCircularBufferPowerOfTwo(unsigned int _bufferLengthPowerOfTwo)
	{
		// --- reset to top
		writeIndex = 0;

		// --- find nearest power of 2 for buffer, save it as bufferLength
		bufferLength = _bufferLengthPowerOfTwo;

		// --- save (bufferLength - 1) for use as wrapping mask
		wrapMask = bufferLength - 1;

		// --- create new buffer
		buffer.reset(new T[bufferLength]);

		// --- flush buffer
		flushBuffer();
	}

	/** write a value into the buffer; this overwrites the previous oldest value in the buffer */
	void writeBuffer(T input)
	{
		// --- write and increment index counter
		buffer[writeIndex++] = input;

		// --- wrap if index > bufferlength - 1
		writeIndex &= wrapMask;
	}

	/** read an arbitrary location that is delayInSamples old */
	T readBuffer(int delayInSamples)//, bool readBeforeWrite = true)
	{
		// --- subtract to make read index
		//     note: -1 here is because we read-before-write,
		//           so the *last* write location is what we use for the calculation
		int readIndex = (writeIndex - 1) - delayInSamples;

		// --- autowrap index
		readIndex &= wrapMask;

		// --- read it
		return buffer[readIndex];
	}

	/** read an arbitrary location that includes a fractional sample */
	T readBuffer(double delayInFractionalSamples)
	{
		// --- truncate delayInFractionalSamples and read the int part
		T y1 = readBuffer((int)delayInFractionalSamples);

		// --- if no interpolation, just return value
		if (!interpolate) return y1;

		// --- else do interpolation
		//
		// --- read the sample at n+1 (one sample OLDER)
		T y2 = readBuffer((int)delayInFractionalSamples + 1);

		// --- get fractional part
		double fraction = delayInFractionalSamples - (int)delayInFractionalSamples;

		// --- do the interpolation (you could try different types here)
		return doLinearInterpolation(y1, y2, fraction);
	}

	/** enable or disable interpolation; usually used for diagnostics or in algorithms that require strict integer samples times */
	void setInterpolate(bool b) { interpolate = b; }

  unsigned int getBufferLength() { return bufferLength; }

private:
	std::unique_ptr<T[]> buffer = nullptr;	///< smart pointer will auto-delete
	unsigned int writeIndex = 0;		///> write index
	unsigned int bufferLength = 1024;	///< must be nearest power of 2
	unsigned int wrapMask = bufferLength - 1;		///< must be (bufferLength - 1)
	bool interpolate = true;			///< interpolation (default is ON)
};

struct ChainSettings {
	float delayTimeLeft {0};
	float delayTimeRight {0};
	float depth {0};
	float rate {0};
	bool dualDelay {true};
	bool chorus {false};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

using Filter = juce::dsp::IIR::Filter<float>;

using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

//==============================================================================
/**
*/
class ChorusAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    ChorusAudioProcessor();
    ~ChorusAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // custom layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;

	ApplicationProperties& getAppProperties() { return appProperties; }

private:
	ApplicationProperties appProperties;

	void updateFilters();
	void applyChorus(int sample, bool left);
	float smoothValues(float current, juce::LinearSmoothedValue<float> smoothed, float next);

	MonoChain leftChain, rightChain;
	juce::LinearSmoothedValue<float> smoothedDelayTimeLeft, smoothedDelayTimeRight, smoothedChorusDepth, smoothedChorusRate;

	CircularBuffer<float> circBuffLeft;
	CircularBuffer<float> circBuffRight;
	double currentSampleRate;

	int writeIndexLeft = 0;
	int writeIndexRight = 0;
	float lastDelayTimeLeft = 100.0f;
	float lastDelayTimeRight = 100.0f;
	float coeff;
	float coeff_chrs;
	float delayTimeLeft;
	float delayTimeRight;

	float chorusRate; 
	float chorusDepth;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusAudioProcessor)
};
