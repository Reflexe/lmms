/*
 * Copyright (c) 2018 Shmuel H. (shmuelhazan0/at/gmail.com)
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
#include "include/SampleBufferVisualizer.h"

#include "Track.h"
#include "Engine.h"

#include <QPainter>
#include <QDebug>

SampleBufferVisualizer::SampleBufferVisualizer() = default;

void SampleBufferVisualizer::update(ReaderType reader, MidiTime sampleStartOffset,
									MidiTime sampleLength, const QRect &parentRect, float pixelsPerTact,
									f_cnt_t framesPerTact, const QPen &pen)
{
	auto buffer = reader.read(reader.read_space());
	
	return update(DataType{&buffer[0], static_cast<f_cnt_t>(buffer.size())},
			sampleStartOffset,
			sampleLength,
			parentRect,
			pixelsPerTact,
			framesPerTact,
			pen);
}


void SampleBufferVisualizer::update(const SampleBuffer &buffer, MidiTime sampleStartOffset, MidiTime sampleLength,
									const QRect &parentRect, float pixelsPerTact, f_cnt_t framesPerTact,
									const QPen &pen) 
{
	return update(DataType{buffer.data(), buffer.frames()},
				  sampleStartOffset,
				  sampleLength,
				  parentRect,
				  pixelsPerTact,
				  framesPerTact,
				  pen);
}

void
SampleBufferVisualizer::update(const DataType& data, MidiTime sampleStartOffset, MidiTime sampleLength,
							   const QRect &parentRect, float pixelsPerTact, f_cnt_t framesPerTact, const QPen &pen) {
	bool shouldClear = false;

	// !=
	if (pixelsPerTact < m_pixelsPerTact || pixelsPerTact > m_pixelsPerTact
		|| framesPerTact != m_framesPerTact
		|| sampleLength < m_cachedTime)
		shouldClear = true;

	m_pixelsPerTact = pixelsPerTact;
	m_generalPaintOffset = sampleStartOffset;
	m_framesPerTact = framesPerTact;
	m_drawPixelOffset = parentRect.left();

	if (shouldClear) {
		clear();
	}


	appendMultipleBars(data,
					   sampleLength,
					   parentRect.translated(-parentRect.x(),
											 0),
					   pen);
}


void SampleBufferVisualizer::draw(QPainter &painter)
{
	float pixelOffset = pixelsPerTime(m_generalPaintOffset) + m_drawPixelOffset;
	for (const auto &pixmap : m_cachedPixmaps) {
		auto targetRect = pixmap.rect.translated(int(pixelOffset),
												 0);

		painter.drawPixmap(targetRect, pixmap.pixmap);
		pixelOffset += pixelsPerTime(pixmap.totalTime);
	}

	auto targetRect = m_currentPixmap.rect.translated(int(pixelOffset),
													  0);
	painter.drawPixmap(targetRect, m_currentPixmap.pixmap);
}

void SampleBufferVisualizer::clear() {
	m_cachedPixmaps.clear();
	m_currentPixmap.pixmap = QPixmap();
	m_currentPixmap.totalTime = 0;
	m_cachedTime = 0;
}


void SampleBufferVisualizer::appendMultipleBars(const DataType &data,
												MidiTime sampleLength,
												const QRect &parentRect,
												const QPen &pen)
{
	for (; (m_cachedTime+m_currentPixmap.totalTime) < sampleLength;)
	{
		MidiTime totalTime;
		MidiTime offsetFromTact = m_currentPixmap.totalTime;
		bool isCompleteTact = false;

		// we have more tacts to draw. finish the current one.
		if (MidiTime(m_cachedTime+offsetFromTact).getBar() < sampleLength.getBar()) {
			// Paint what left from the current tact.
			totalTime = MidiTime::ticksPerBar() - offsetFromTact;
			isCompleteTact = true;
		} else {
			// Draw only the ticks left in the current tact.
			totalTime = sampleLength - m_cachedTime - m_currentPixmap.totalTime;
		}

		Q_ASSERT((offsetFromTact + totalTime) <= MidiTime::ticksPerBar());

		if (pixelsPerTime(totalTime) < 1) {
			// We can't paint it.
			// totalTime is too short. skip it.
			// or just wait until we have enough frames.
			if (isCompleteTact) {
				// Skip it and continue to the next tact.
				m_currentPixmap.totalTime += totalTime;
			} else {
				// Wait until we have enough frames.
				break;
			}
		}

		auto result = appendBar(data,
								totalTime,
								parentRect,
								pen,
								isCompleteTact);
		if (! result)
			break;

		if (isCompleteTact) {
			m_cachedTime += ( m_currentPixmap.totalTime );
			m_cachedPixmaps.push_back(m_currentPixmap);
			m_currentPixmap.pixmap = QPixmap();
			m_currentPixmap.totalTime = 0;
		}
	}
}

bool SampleBufferVisualizer::appendBar(const DataType &data,
									   const MidiTime &totalTime,
									   const QRect &parentRect,
									   const QPen &pen,
									   bool isLastInTact)
{
	auto offsetFromTact = m_currentPixmap.totalTime;

	auto currentPaintInTact = getRectForSampleFragment (parentRect,
														offsetFromTact,
														totalTime,
														isLastInTact);
	Q_ASSERT(currentPaintInTact.width() > 0);

	// Generate the actual visualization.
	auto fromFrame = MidiTime(m_cachedTime + offsetFromTact).frames (m_framesPerTact);

	auto poly = visualizeToPoly(data, currentPaintInTact,
								QRect(),
								fromFrame,
								fromFrame + totalTime.frames(m_framesPerTact));


	m_currentPixmap.totalTime += totalTime;

	m_currentPixmap.rect = getRectForSampleFragment (parentRect,
													 0,
													 MidiTime::ticksPerBar());
	if (m_currentPixmap.pixmap.isNull()) {
		m_currentPixmap.pixmap = QPixmap(m_currentPixmap.rect.size());
		m_currentPixmap.pixmap.fill(Qt::transparent);
	}

	// Draw the points into the pixmap.
	QPainter pixmapPainter (&m_currentPixmap.pixmap);
	pixmapPainter.setPen(pen);

	pixmapPainter.setRenderHint( QPainter::Antialiasing );

	pixmapPainter.drawPolyline (poly.first);
	pixmapPainter.drawPolyline (poly.second);
	pixmapPainter.end();

	// Continue to the next tact or stop.
	return true;
}

QRect SampleBufferVisualizer::getRectForSampleFragment(QRect parentRect, MidiTime beginOffset,
													   MidiTime totalTime,
													   bool forceNotZeroWidth) {
	int offset = pixelsPerTime(beginOffset);

	float top = parentRect.top ();
	float height = parentRect.height ();

	QRect r = QRect( int(parentRect.x ()) + int(offset),
					 top,
					 int(qMax( int(pixelsPerTime(totalTime)) , (forceNotZeroWidth ? 1 : 0) )),
					 int(height));


	return r;
}

//void SampleBufferVisualizer::visualize(QPainter &_p, const QRect &_dr,
//							 const QRect &_clip, f_cnt_t _from_frame, f_cnt_t _to_frame) {
////	auto polyPair = visualizeToPoly(<#initializer#>, _dr, _clip, _from_frame, _to_frame);
//
//	_p.setRenderHint(QPainter::Antialiasing);
//	_p.drawPolyline(polyPair.first);
//	_p.drawPolyline(polyPair.second);
//}

std::pair<QPolygonF, QPolygonF>
SampleBufferVisualizer::visualizeToPoly(const DataType &data, const QRect &_dr, const QRect &_clip, f_cnt_t _from_frame,
										f_cnt_t _to_frame) const {
	
	const int w = _dr.width();
	const int h = _dr.height();
	const bool focus_on_range = _from_frame < _to_frame;
	int y_space = (h / 2);

	/* Don't visualize while rendering / doing after-rendering changes. */
	if (data.frames == 0) return {};

	auto to_frame = qMin<f_cnt_t>(_to_frame, data.frames);

	const int nb_frames = focus_on_range ? to_frame - _from_frame : data.frames;
	if (nb_frames == 0) return {};

	const int fpp = qBound<int>(1, nb_frames / w,  20);

	bool shouldAddAdditionalPoint = (nb_frames % fpp) != 0;
	int pointsCount = (nb_frames / fpp) + (shouldAddAdditionalPoint ? 1 : 0);
	auto l = QPolygonF(pointsCount);
	auto r = QPolygonF(pointsCount);

	int n = 0;
	const int xb = _dr.x();
	const int first = focus_on_range ? _from_frame : 0;
	const int last = focus_on_range ? to_frame : data.frames;

	int zeroPoint = _dr.y() + y_space;
	if (h % 2 != 0)
		zeroPoint += 1;
	for (int frame = first; frame < last; frame += fpp) {
		double x = (xb + (frame - first) * double(w) / nb_frames);

		l[n] = QPointF(x,
					   (zeroPoint + (data.data[frame][0] * y_space)));
		r[n] = QPointF(x,
					   (zeroPoint + (data.data[frame][1] * y_space)));

		++n;
	}

	return {std::move(l), std::move(r)};
}
