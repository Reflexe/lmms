/*
 * SampleBuffer.h - container-class SampleBuffer
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#ifndef SAMPLE_BUFFER_H
#define SAMPLE_BUFFER_H

#include "internal/SampleBufferData.h"
#include "internal/SampleBufferPlayInfo.h"
#include "JournallingObject.h"
#include "Threading.h"

class QPainter;

class QRect;

// values for buffer margins, used for various libsamplerate interpolation modes
// the array positions correspond to the converter_type parameter values in libsamplerate
// if there appears problems with playback on some interpolation mode, then the value for that mode
// may need to be higher - conversely, to optimize, some may work with lower values
const f_cnt_t MARGIN[] = {64, 64, 64, 4, 4};

class LMMS_EXPORT SampleBuffer : public QObject, public JournallingObject {
Q_OBJECT
MM_OPERATORS

	typedef std::shared_ptr<internal::SampleBufferData> SharedData;

public:
	using DataVector=internal::SampleBufferData::DataVector;
	using LoopMode=::LoopMode;

	class LMMS_EXPORT handleState {
	MM_OPERATORS

	public:
		handleState(bool _varying_pitch = false, int interpolation_mode = SRC_LINEAR);

		virtual ~handleState();

		f_cnt_t frameIndex() const {
			return m_frameIndex;
		}

		void setFrameIndex(f_cnt_t _index) {
			m_frameIndex = _index;
		}

		bool isBackwards() const {
			return m_isBackwards;
		}

		void setBackwards(bool _backwards) {
			m_isBackwards = _backwards;
		}

		int interpolationMode() const {
			return m_interpolationMode;
		}


		f_cnt_t m_frameIndex;
		const bool m_varyingPitch;
		bool m_isBackwards;
		SRC_STATE *m_resamplingData;
		int m_interpolationMode;
	};


	explicit SampleBuffer();

	explicit SampleBuffer(internal::SampleBufferData &&data);

	/**
	 * Load this sample buffer from @a _audio_file,
	 * @param _audio_file	path to an audio file.
	 */
	explicit SampleBuffer(const QString &_audio_file, bool ignoreError=false);

	/**
	 * Load the sample buffer from a base64 encoded string.
	 * @param base64Data	The data.
	 * @param sample_rate	The data's sample rate.
	 */
	SampleBuffer(const QString &base64Data, sample_rate_t sample_rate);

	SampleBuffer(DataVector &&movedData, sample_rate_t sampleRate);

	inline virtual QString nodeName() const override {
		return "samplebuffer";
	}

	SampleBuffer(SampleBuffer &&sampleBuffer) noexcept;

	SampleBuffer &operator=(SampleBuffer &&other) noexcept;

	virtual void saveSettings(QDomDocument &doc, QDomElement &_this) override;

	virtual void loadSettings(const QDomElement &_this) override;

	bool play(sampleFrame *_ab, handleState *_state,
			  SampleBufferPlayInfo &playInfo,
			  const fpp_t _frames,
			  const float _freq,
			  const LoopMode _loopmode = LoopMode::LoopOff);

//	void setAudioFile(const QString &audioFile, bool ignoreError = false);

	static QString openAudioFile(const QString &currentAudioFile = QString());

	static QString openAndSetWaveformFile(QString currentAudioFile = QString());

	sample_t userWaveSample(const float _sample) const;


	static QString tryToMakeRelative(const QString &_file);

	static QString tryToMakeAbsolute(const QString &file);

//	/**
//	 * @brief Add data to the buffer,
//	 * @param begin	Beginning of an InputIterator.
//	 * @param end	End of an InputIterator.
//	 * @return
//	 * @todo remove
//	 */
//	void addData(const DataVector &vector, sample_rate_t sampleRate);
//
//	/**
//	 * @brief Reset the class and initialize it with @a newData.
//	 * @param newData	mm, that's the new data.
//	 * @param dataSampleRate	Sample rate for @a newData.
//	 * @todo remove
//	 */
//	void resetData(DataVector &&newData, sample_rate_t dataSampleRate);
//
//	/**
//	 * @brief Just reverse the current buffer.
//	 *
//	 * This function simply calls `std::reverse` on m_data.
//	 * @todo remove (should be a play info property)
//	 */
//	void reverse();
	
	sample_rate_t sampleRate() const 
	{
		return m_data->getSampleRate();
	}
	
	/* TODO */
	QString audioFileName() const 
	{
		return QString();
	}
	
	f_cnt_t frames() const
	{
		return m_data->frames();
	}
	
	const sampleFrame *data() const 
	{
		return m_data->data();
	}
	
private:

	QString &toBase64(QString &_dst) const;

	// HACK: libsamplerate < 0.1.8 doesn't get read-only variables
	//	     as const. It has been fixed in 0.1.9 but has not been
	//		 shipped for some distributions.
	//		 This function just returns a variable that should have
	//		 been `const` as non-const to bypass using 0.1.9.
	inline static sampleFrame *libSampleRateSrc(const sampleFrame *ptr) {
		return const_cast<sampleFrame *>(ptr);
	}

	static f_cnt_t getLoopedIndex(f_cnt_t _index, f_cnt_t _startf, f_cnt_t _endf);

	static f_cnt_t getPingPongIndex(f_cnt_t _index, f_cnt_t _startf, f_cnt_t _endf);

	/**
	 * @brief A helper class used when changes to m_data is needed
	 */
//	class DataChangeHelper {
//	public:
//		explicit DataChangeHelper(SampleBuffer *buffer, UpdateType updateType);
//
//		~DataChangeHelper();
//
//	private:
//		SampleBuffer *m_buffer;
//		UpdateType m_updateType;
//		Mixer::RequestChangesGuard m_mixerGuard;
//	};
//
signals:
	void sampleUpdated();

private:
	SharedData m_data;
};


#endif
