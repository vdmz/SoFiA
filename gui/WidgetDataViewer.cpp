/// ____________________________________________________________________ ///
///                                                                      ///
/// SoFiA 1.0.0 (WidgetDataViewer.cpp) - Source Finding Application      ///
/// Copyright (C) 2016-2017 Tobias Westmeier                             ///
/// ____________________________________________________________________ ///
///                                                                      ///
/// Address:  Tobias Westmeier                                           ///
///           ICRAR M468                                                 ///
///           The University of Western Australia                        ///
///           35 Stirling Highway                                        ///
///           Crawley WA 6009                                            ///
///           Australia                                                  ///
///                                                                      ///
/// E-mail:   tobias.westmeier [at] uwa.edu.au                           ///
/// ____________________________________________________________________ ///
///                                                                      ///
/// This program is free software: you can redistribute it and/or modify ///
/// it under the terms of the GNU General Public License as published by ///
/// the Free Software Foundation, either version 3 of the License, or    ///
/// (at your option) any later version.                                  ///
///                                                                      ///
/// This program is distributed in the hope that it will be useful,      ///
/// but WITHOUT ANY WARRANTY; without even the implied warranty of       ///
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         ///
/// GNU General Public License for more details.                         ///
///                                                                      ///
/// You should have received a copy of the GNU General Public License    ///
/// along with this program. If not, see http://www.gnu.org/licenses/.   ///
/// ____________________________________________________________________ ///
///                                                                      ///

#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include "WidgetDataViewer.h"

// ----------- //
// CONSTRUCTOR //
// ----------- //

WidgetDataViewer::WidgetDataViewer(const std::string &url, QWidget *parent) : QWidget(parent, Qt::Window)
{
	this->setParent(parent);
	this->setWindowTitle("SoFiA - Image Viewer");
	this->setAttribute(Qt::WA_DeleteOnClose);
	
	scale  = 2.0;
	offset = 0.0;
	dataMin = 0.0;
	dataMax = 1.0;
	plotMin = 0.0;
	plotMax = 1.0;
	
	revert = 0x00;
	invert = 0x00;
	currentLut = RAINBOW;
	transferFunction = LIN;  // linear by default
	currentChannel = 0;
	
	setUpInterface();
	fieldChannel->setText(QString::number(currentChannel));
	
	fips = new Fips;
	
	if(not openFitsFile(url))
	{
		plotMin = dataMin;
		plotMax = dataMax;
		plotChannelMap(currentChannel);
	}
	
	fieldLevelMin->setText(QString::number(plotMin));
	fieldLevelMax->setText(QString::number(plotMax));
	
	return;
}



// ---------- //
// DESTRUCTOR //
// ---------- //

WidgetDataViewer::~WidgetDataViewer()
{
	delete fips;
	return;
}



// ---------------------------- //
// FUNCTION to open a FITS file //
// ---------------------------- //

int WidgetDataViewer::openFitsFile(const std::string &url)
{
	if(fips->readFile(url)) return 1;
	
	// Redefine settings
	scale = std::max(static_cast<double>(viewport->width()) / static_cast<double>(fips->dimension(1)), static_cast<double>(viewport->height()) / static_cast<double>(fips->dimension(2)));
	
	if(std::isnan(fips->minimum()) or std::isnan(fips->maximum()) or fips->minimum() >= fips->maximum())
	{
		std::cerr << "Warning: DATAMIN or DATAMAX undefined; calculating values.\n";
		
		dataMin =  std::numeric_limits<double>::max();
		dataMax = -std::numeric_limits<double>::max();
		
		for(size_t z = 0; z < (fips->dimension() > 2 ? fips->dimension(3) : 1); ++z)
		{
			for(size_t y = 0; y < fips->dimension(2); ++y)
			{
				for(size_t x = 0; x < fips->dimension(1); ++x)
				{
					size_t pos[3] = {x, y, z};
					double value = fips->data(pos);
					
					if(not std::isnan(value))
					{
						if(value < dataMin) dataMin = value;
						if(value > dataMax) dataMax = value;
					}
				}
			}
		}
		
		if(dataMin >= dataMax)
		{
			std::cerr << "Warning: DATAMIN not greater than DATAMAX; using default values.\n";
			dataMin = 0.0;
			dataMax = 1.0;
		}
	}
	else
	{
		dataMin = fips->minimum();
		dataMax = fips->maximum();
	}
	
	//std::cout << "DATAMIN = " << dataMin << "\n";
	//std::cout << "DATAMAX = " << dataMax << "\n";
	
	// Adjust user interface
	slider->setMaximum(fips->dimension() < 3 ? 0 : fips->dimension(3) - 1);
	slider->setEnabled(fips->dimension() > 2);
	buttonFirst->setEnabled(fips->dimension() > 2);
	buttonPrev->setEnabled(fips->dimension() > 2);
	buttonNext->setEnabled(fips->dimension() > 2);
	buttonLast->setEnabled(fips->dimension() > 2);
	
	return 0;
}



// ------------------------------------------ //
// FUNCTION to draw channel map onto viewport //
// ------------------------------------------ //

int WidgetDataViewer::plotChannelMap(size_t z)
{
	if(not fips->dimension()) return 1;
	
	if(fips->dimension() < 3) z = 0;
	else if(z >= fips->dimension(3)) z = fips->dimension(3) - 1;
	
	for(size_t b = 0; b < VIEWPORT_HEIGHT; ++b)
	{
		for(size_t a = 0; a < VIEWPORT_WIDTH; ++a)
		{
			long x = floor(static_cast<double>(a) / scale - offset);
			long y = floor(static_cast<double>(viewport->height() - b - 1) / scale - offset);
			
			if(x >= 0 and static_cast<size_t>(x) < fips->dimension(1) and y >= 0 and static_cast<size_t>(y) < fips->dimension(2))
			{
				size_t position[3] = {static_cast<size_t>(x), static_cast<size_t>(y), z};
				double value = fips->data(position);
				unsigned int index = flux2grey(value);
				
				image->setPixel(a, b, index xor revert);
			}
			else
			{
				image->setPixel(a, b, 0);
			}
		}
	}
	
	viewport->setPixmap(QPixmap::fromImage(*image));
	
	return 0;
}



// ----------------------------- //
// SLOT to show previous channel //
// ----------------------------- //

void WidgetDataViewer::showPrevChannel()
{
	if(not fips->dimension()) return;
	if(currentChannel > 0) --currentChannel;
	fieldChannel->setText(QString::number(currentChannel));
	bool sliderBlocked = slider->blockSignals(true);
	slider->setValue(currentChannel);
	slider->blockSignals(sliderBlocked);
	plotChannelMap(currentChannel);
	return;
}



// ------------------------- //
// SLOT to show next channel //
// ------------------------- //

void WidgetDataViewer::showNextChannel()
{
	if(not fips->dimension()) return;
	if(currentChannel < fips->dimension(3) - 1) ++currentChannel;
	fieldChannel->setText(QString::number(currentChannel));
	bool sliderBlocked = slider->blockSignals(true);
	slider->setValue(currentChannel);
	slider->blockSignals(sliderBlocked);
	plotChannelMap(currentChannel);
	return;
}



// -------------------------- //
// SLOT to show first channel //
// -------------------------- //

void WidgetDataViewer::showFirstChannel()
{
	if(not fips->dimension()) return;
	currentChannel = 0;
	fieldChannel->setText(QString::number(currentChannel));
	bool sliderBlocked = slider->blockSignals(true);
	slider->setValue(currentChannel);
	slider->blockSignals(sliderBlocked);
	plotChannelMap(currentChannel);
	return;
}



// ------------------------- //
// SLOT to show last channel //
// ------------------------- //

void WidgetDataViewer::showLastChannel()
{
	if(not fips->dimension()) return;
	currentChannel = fips->dimension(3) - 1;
	fieldChannel->setText(QString::number(currentChannel));
	bool sliderBlocked = slider->blockSignals(true);
	slider->setValue(currentChannel);
	slider->blockSignals(sliderBlocked);
	plotChannelMap(currentChannel);
	return;
}



// -------------------------------- //
// SLOT to react to slider movement //
// -------------------------------- //

void WidgetDataViewer::sliderChange(int value)
{
	if(not fips->dimension()) return;
	currentChannel = value;
	fieldChannel->setText(QString::number(currentChannel));
	plotChannelMap(currentChannel);
	return;
}



// ------------------------------------------------- //
// FUNCTION to turn flux into 8-bit grey-scale value //
// ------------------------------------------------- //

unsigned int WidgetDataViewer::flux2grey(double value)
{
	if(std::isnan(value) or value < plotMin) return 0;
	else if(value > plotMax) return 255;
	
	switch(transferFunction)
	{
		case SQRT:
			return static_cast<unsigned int>(255.0 * sqrt((value - plotMin) / (plotMax - plotMin)));
		case LOG:
			return static_cast<unsigned int>(255.0 * log10(9.0 * (value - plotMin) / (plotMax - plotMin) + 1.0));
	}
	
	// Default for linear transfer function
	return static_cast<unsigned int>(255.0 * (value - plotMin) / (plotMax - plotMin));
}



// --------------------------------- //
// FUNCTION to set up user interface //
// --------------------------------- //

void WidgetDataViewer::setUpInterface()
{
	// Set up icons
	iconGoPreviousView.addFile(QString(":/icons/22/go-previous-view.png"), QSize(22, 22));
	iconGoPreviousView.addFile(QString(":/icons/16/go-previous-view.png"), QSize(16, 16));
	iconGoPreviousView  = QIcon::fromTheme("go-previous-view", iconGoPreviousView);
	
	iconGoNextView.addFile(QString(":/icons/22/go-next-view.png"), QSize(22, 22));
	iconGoNextView.addFile(QString(":/icons/16/go-next-view.png"), QSize(16, 16));
	iconGoNextView      = QIcon::fromTheme("go-next-view", iconGoNextView);
	
	iconGoFirstView.addFile(QString(":/icons/22/go-first-view.png"), QSize(22, 22));
	iconGoFirstView.addFile(QString(":/icons/16/go-first-view.png"), QSize(16, 16));
	iconGoFirstView     = QIcon::fromTheme("go-first-view", iconGoNextView);
	
	iconGoLastView.addFile(QString(":/icons/22/go-last-view.png"), QSize(22, 22));
	iconGoLastView.addFile(QString(":/icons/16/go-last-view.png"), QSize(16, 16));
	iconGoLastView      = QIcon::fromTheme("go-last-view", iconGoNextView);
	
	iconFillColor.addFile(QString(":/icons/22/fill-color.png"), QSize(22, 22));
	iconFillColor.addFile(QString(":/icons/16/fill-color.png"), QSize(16, 16));
	iconFillColor       = QIcon::fromTheme("fill-color", iconGoNextView);
	
	iconDialogClose.addFile(QString(":/icons/22/dialog-close.png"), QSize(22, 22));
	iconDialogClose.addFile(QString(":/icons/16/dialog-close.png"), QSize(16, 16));
	iconDialogClose = QIcon::fromTheme("dialog-close", iconDialogClose);
	
	iconEditCopy.addFile(QString(":/icons/22/edit-copy.png"), QSize(22, 22));
	iconEditCopy.addFile(QString(":/icons/16/edit-copy.png"), QSize(16, 16));
	iconEditCopy = QIcon::fromTheme("edit-copy", iconEditCopy);
	
	// Set up data display image
	image = new QImage(VIEWPORT_WIDTH, VIEWPORT_HEIGHT, QImage::Format_Indexed8);
	setUpLut(RAINBOW);
	image->setColorTable(lut);
	image->fill(0);
	
	// Set up image viewport:
	viewport = new QLabel(this);
	viewport->setPixmap(QPixmap::fromImage(*image));
	viewport->setMaximumWidth(VIEWPORT_WIDTH);
	viewport->setMinimumWidth(VIEWPORT_WIDTH);
	viewport->setMaximumHeight(VIEWPORT_HEIGHT);
	viewport->setMinimumHeight(VIEWPORT_HEIGHT);
	viewport->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	viewport->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(viewport, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(showContextMenu(const QPoint &)));
	
	// Set up settings widget
	settings = new QWidget(this);
	labelLevelMin = new QLabel(settings);
	labelLevelMin->setText("Min:");
	labelLevelMin->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	labelLevelMax = new QLabel(settings);
	labelLevelMax->setText("Max:");
	labelLevelMax->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	fieldLevelMin = new QLineEdit(settings);
	//fieldLevelMin->setMaximumWidth(50);
	fieldLevelMin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	fieldLevelMin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	connect(fieldLevelMin, SIGNAL(editingFinished()), this, SLOT(setLevelMin()));
	fieldLevelMax = new QLineEdit(settings);
	//fieldLevelMax->setMaximumWidth(50);
	fieldLevelMax->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	fieldLevelMax->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	connect(fieldLevelMax, SIGNAL(editingFinished()), this, SLOT(setLevelMax()));
	checkRev = new QCheckBox("rev", settings);
	connect(checkRev, SIGNAL(stateChanged(int)), this, SLOT(toggleRev(int)));
	checkInv = new QCheckBox("inv", settings);
	connect(checkInv, SIGNAL(stateChanged(int)), this, SLOT(toggleInv(int)));
	fieldTransFunc = new QComboBox(settings);
	fieldTransFunc->insertItem(LIN,  "linear");
	fieldTransFunc->insertItem(SQRT, "sqrt");
	fieldTransFunc->insertItem(LOG,  "log");
	connect(fieldTransFunc, SIGNAL(currentIndexChanged(int)), this, SLOT(setTransferFunction(int)));
	buttonReset = new QToolButton(settings);
	buttonReset->setToolButtonStyle(Qt::ToolButtonTextOnly);
	buttonReset->setText("Reset");
	buttonReset->setToolTip("Reset display settings");
	//buttonFirst->setIcon(iconGoFirstView);
	buttonReset->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonReset, SIGNAL(clicked()), this, SLOT(resetDisplaySettings()));
	
	layoutSettings = new QHBoxLayout;
	layoutSettings->addWidget(labelLevelMin);
	layoutSettings->addWidget(fieldLevelMin);
	layoutSettings->addWidget(labelLevelMax);
	layoutSettings->addWidget(fieldLevelMax);
	layoutSettings->addWidget(checkRev);
	layoutSettings->addWidget(checkInv);
	layoutSettings->addWidget(fieldTransFunc);
	layoutSettings->addWidget(buttonReset);
	layoutSettings->setContentsMargins(0, 0, 0, 0);
	layoutSettings->setSpacing(5);
	settings->setLayout(layoutSettings);
	
	// Set up status widget
	status = new QLabel(this);
	status->setText("Status information");
	
	// Set up control widget
	controls = new QWidget(this);
	buttonFirst  = new QToolButton(controls);
	buttonFirst->setToolButtonStyle(Qt::ToolButtonIconOnly);
	buttonFirst->setEnabled(false);
	buttonFirst->setText("First");
	buttonFirst->setToolTip("First channel");
	buttonFirst->setIcon(iconGoFirstView);
	buttonFirst->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonFirst, SIGNAL(clicked()), this, SLOT(showFirstChannel()));
	buttonLast   = new QToolButton(controls);
	buttonLast->setToolButtonStyle(Qt::ToolButtonIconOnly);
	buttonLast->setEnabled(false);
	buttonLast->setText("Last");
	buttonLast->setToolTip("Last channel");
	buttonLast->setIcon(iconGoLastView);
	buttonLast->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonLast, SIGNAL(clicked()), this, SLOT(showLastChannel()));
	buttonPrev   = new QToolButton(controls);
	buttonPrev->setToolButtonStyle(Qt::ToolButtonIconOnly);
	buttonPrev->setEnabled(false);
	buttonPrev->setText("Prev.");
	buttonPrev->setToolTip("Previous channel");
	buttonPrev->setIcon(iconGoPreviousView);
	buttonPrev->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonPrev, SIGNAL(clicked()), this, SLOT(showPrevChannel()));
	buttonNext   = new QToolButton(controls);
	buttonNext->setToolButtonStyle(Qt::ToolButtonIconOnly);
	buttonNext->setEnabled(false);
	buttonNext->setText("Next");
	buttonNext->setToolTip("Next channel");
	buttonNext->setIcon(iconGoNextView);
	buttonNext->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonNext, SIGNAL(clicked()), this, SLOT(showNextChannel()));
	fieldChannel = new QLineEdit(controls);
	fieldChannel->setMaximumWidth(50);
	fieldChannel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	fieldChannel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	fieldChannel->setReadOnly(true);
	slider       = new QSlider(Qt::Horizontal, controls);
	slider->setEnabled(false);
	slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	slider->setMinimum(0);
	slider->setMaximum(100);
	slider->setSingleStep(1);
	slider->setPageStep(10);
	connect(slider, SIGNAL(valueChanged(int)), this, SLOT(sliderChange(int)));
	buttonClose  = new QToolButton(controls);
	buttonClose->setToolButtonStyle(Qt::ToolButtonIconOnly);
	buttonClose->setEnabled(true);
	buttonClose->setText("Close");
	buttonClose->setToolTip("Close viewer");
	buttonClose->setIcon(iconDialogClose);
	buttonClose->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	connect(buttonClose, SIGNAL(clicked()), this, SLOT(close()));
	
	layoutControls = new QHBoxLayout;
	layoutControls->addWidget(buttonFirst);
	layoutControls->addWidget(buttonPrev);
	layoutControls->addWidget(fieldChannel);
	layoutControls->addWidget(buttonNext);
	layoutControls->addWidget(buttonLast);
	layoutControls->addWidget(slider);
	layoutControls->addWidget(buttonClose);
	layoutControls->setContentsMargins(0, 0, 0, 0);
	layoutControls->setSpacing(5);
	controls->setLayout(layoutControls);
	
	// Assemble main window layout
	mainLayout = new QVBoxLayout;
	mainLayout->addWidget(settings);
	mainLayout->addWidget(viewport);
	mainLayout->addWidget(controls);
	mainLayout->addWidget(status);
	mainLayout->setContentsMargins(5, 5, 5, 5);
	mainLayout->setSpacing(5);
	this->setLayout(mainLayout);
	
	// Set up event filters
	viewport->setMouseTracking(true);
	viewport->installEventFilter(this);
	
	this->show();
	
	return;
}



// --------------------------------- //
// FUNCTION to set up look-up tables //
// --------------------------------- //

void WidgetDataViewer::setUpLut(int type)
{
	lut.clear();
	srand(10);
	unsigned int valueR, valueG, valueB;
	
	for(unsigned int i = 0; i < 256; ++i)
	{
		switch(type)
		{
			case 1:
				// RAINBOW
				if (i <  50) lut.append(qRgb(0 xor invert, static_cast<unsigned int>(255.0 * (static_cast<double>(i) - 0.0) / 50.0) xor invert, 255 xor invert));
				else if(i < 125) lut.append(qRgb(0 xor invert, 255 xor invert, static_cast<unsigned int>(255.0 * (125.0 - static_cast<double>(i)) / 75.0) xor invert));
				else if(i < 180) lut.append(qRgb(static_cast<unsigned int>(255 * (static_cast<double>(i) - 125) / 55) xor invert, 255 xor invert ,0 xor invert));
				else lut.append(qRgb(255 xor invert, static_cast<unsigned int>(255 * (255 - static_cast<double>(i)) / 75) xor invert, 0 xor invert));
				break;
			
			case 2:
				// RANDOM
				valueR = static_cast<unsigned int>(255.0 * static_cast<double>(rand()) / static_cast<double>(RAND_MAX));
				valueG = static_cast<unsigned int>(255.0 * static_cast<double>(rand()) / static_cast<double>(RAND_MAX));
				valueB = static_cast<unsigned int>(255.0 * static_cast<double>(rand()) / static_cast<double>(RAND_MAX));
				lut.append(qRgb(valueR xor invert, valueG xor invert, valueB xor invert));
				break;
			
			default:
				// GREYSCALE
				lut.append(qRgb(i xor invert, i xor invert, i xor invert));
		}
	}
	
	image->setColorTable(lut);
	return;
}

	

// ----------------------------- //
// SLOTs to change look-up table //
// ----------------------------- //

void WidgetDataViewer::selectLutGreyscale()
{
	currentLut = GREYSCALE;
	setUpLut(currentLut);
	plotChannelMap(currentChannel);
	return;
}

void WidgetDataViewer::selectLutRainbow()
{
	currentLut = RAINBOW;
	setUpLut(currentLut);
	plotChannelMap(currentChannel);
	return;
}

void WidgetDataViewer::selectLutRandom()
{
	currentLut = RANDOM;
	setUpLut(currentLut);
	plotChannelMap(currentChannel);
	return;
}



// ------------------------------------------ //
// FUNCTION to set up mouse move event filter //
// ------------------------------------------ //

bool WidgetDataViewer::eventFilter(QObject *obj, QEvent *event)
{
	if(event->type() == QEvent::MouseMove and fips->dimension())
	{
		QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
		
		int a = mouseEvent->pos().x();
		int b = mouseEvent->pos().y();
		
		long x = floor(static_cast<double>(a) / scale - offset);
		long y = floor(static_cast<double>(viewport->height() - b - 1) / scale - offset);
		
		QString text("Undefined");
		
		if(x >= 0 and static_cast<size_t>(x) < fips->dimension(1) and y >= 0 and static_cast<size_t>(y) < fips->dimension(2))
		{
			size_t position[3] = {static_cast<size_t>(x), static_cast<size_t>(y), currentChannel};
			double value = fips->data(position);
			
			if(std::isnan(value))
			{
				text = QString("Position: %1, %2   Value: blank").arg(x).arg(y);
			}
			else
			{
				text = QString("Position: %1, %2   Value: %3 ").arg(x).arg(y).arg(value);
				QString bunit = QString::fromStdString(fips->unit());
				if(not bunit.isEmpty()) text.append(bunit);
			}
		}
		else text = QString("Position: undefined   Value: undefined");
		
		status->setText(text);
		return true;
	}
	
	return QObject::eventFilter(obj, event);
}



// --------------------------- //
// SLOT to create context menu //
// --------------------------- //

void WidgetDataViewer::showContextMenu(const QPoint &where)
{
	QMenu contextMenu("Context Menu", this);
	QMenu menuLut("Colour Scale", this);
	
	QAction actionLutGreyscale("Greyscale", this);
	connect(&actionLutGreyscale, SIGNAL(triggered()), this, SLOT(selectLutGreyscale()));
	QAction actionLutRainbow("Rainbow", this);
	connect(&actionLutRainbow, SIGNAL(triggered()), this, SLOT(selectLutRainbow()));
	QAction actionLutRandom("Random", this);
	connect(&actionLutRandom, SIGNAL(triggered()), this, SLOT(selectLutRandom()));
	
	menuLut.addAction(&actionLutGreyscale);
	menuLut.addAction(&actionLutRainbow);
	menuLut.addAction(&actionLutRandom);
	menuLut.setIcon(iconFillColor);
	
	QAction actionCopy("Copy", this);
	actionCopy.setIcon(iconEditCopy);
	connect(&actionCopy, SIGNAL(triggered()), this, SLOT(copy()));
	
	contextMenu.addAction(&actionCopy);
	contextMenu.addSeparator();
	contextMenu.addMenu(&menuLut);
	
	contextMenu.exec(mapToGlobal(where));
	
	return;
}



// ------------------------- //
// SLOT to set minimum level //
// ------------------------- //

void WidgetDataViewer::setLevelMin()
{
	bool ok;
	double value = (fieldLevelMin->text()).toDouble(&ok);
	
	if(ok and value < plotMax)
	{
		plotMin = value;
		plotChannelMap(currentChannel);
	}
	else
	{
		fieldLevelMin->setText(QString::number(plotMin));
	}
	
	return;
}



// ------------------------- //
// SLOT to set maximum level //
// ------------------------- //

void WidgetDataViewer::setLevelMax()
{
	bool ok;
	double value = (fieldLevelMax->text()).toDouble(&ok);
	
	if(ok and value > plotMin)
	{
		plotMax = value;
		plotChannelMap(currentChannel);
	}
	else
	{
		fieldLevelMax->setText(QString::number(plotMax));
	}
	
	return;
}



// -------------------------------- //
// SLOT to change transfer function //
// -------------------------------- //

void WidgetDataViewer::setTransferFunction(int which)
{
	transferFunction = which;
	plotChannelMap(currentChannel);
	
	return;
}



// ------------------------------- //
// SLOT to toggle reverted colours //
// ------------------------------- //

void WidgetDataViewer::toggleRev(int state)
{
	revert = (state == Qt::Checked) ? 0xFF : 0x00;
	plotChannelMap(currentChannel);
	
	return;
}



// ------------------------------- //
// SLOT to toggle inverted colours //
// ------------------------------- //

void WidgetDataViewer::toggleInv(int state)
{
	invert = (state == Qt::Checked) ? 0xFF : 0x00;
	setUpLut(currentLut);
	plotChannelMap(currentChannel);
	
	return;
}



// ------------------------------ //
// SLOT to reset display settings //
// ------------------------------ //

void WidgetDataViewer::resetDisplaySettings()
{
	plotMin = dataMin;
	plotMax = dataMax;
	revert = 0x00;
	invert = 0x00;
	currentLut = RAINBOW;
	transferFunction = LIN;
	
	fieldLevelMin->setText(QString::number(plotMin));
	fieldLevelMax->setText(QString::number(plotMax));
	checkRev->setCheckState(Qt::Unchecked);
	checkInv->setCheckState(Qt::Unchecked);
	fieldTransFunc->setCurrentIndex(transferFunction);
	
	setUpLut(currentLut);
	plotChannelMap(currentChannel);
	
	return;
}



// ----------------------------------- //
// FUNCTION to copy image to clipboard //
// ----------------------------------- //

void WidgetDataViewer::copy()
{
    if(image and not image->isNull())
	{
		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setImage(*image);
	}
	
	return;
}



// ----------- //
// Close event //
// ----------- //

void WidgetDataViewer::closeEvent(QCloseEvent *event)
{
	event->accept();
	return;
}