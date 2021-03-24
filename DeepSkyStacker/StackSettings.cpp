#include <algorithm>
using std::min;
using std::max;

#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <afx.h>
#include <afxcmn.h>
#include <afxcview.h>
#include <afxwin.h>

#include <QFileDialog>
#include <QSettings>
#include <QShowEvent>

#include <ZExcept.h>
#include <Ztrace.h>

extern bool		g_bShowRefStars;

#include "DSSCommon.h"
#include "commonresource.h"
#include "DSSVersion.h"
#include "DeepSkyStacker.h"
#include "DeepStackerDlg.h"
#include "Workspace.h"

#include "StackSettings.h"
#include "ui/ui_StackSettings.h"
#include "avx_support.h"


StackSettings::StackSettings(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::StackSettings),
	initialised(false),
	pStackingTasks(nullptr),
	registeringOnly(false),
	cometStacking(false),
	enableDark(false),
	enableFlat(false),
	enableBias(false),
	enableAll(false),
	customRectangleSelected(false),
	customRectangleEnabled(false),
	startingTab(-1)
{
    ui->setupUi(this);

	m_resultParameters = new ResultParameters(this);
	m_cometStacking = new CometStacking(this);
	m_alignmentParameters = new AlignmentParameters(this);
	m_intermediateFiles = new IntermediateFiles(this, registeringOnly);
	m_postCalibration = new PostCalibration(this);
	m_outputTab = new OutputTab(this);

	m_lightFrames = new StackingParameters(this, PICTURETYPE_LIGHTFRAME);
	m_darkFrames = new StackingParameters(this, PICTURETYPE_DARKFRAME);
	m_flatFrames = new StackingParameters(this, PICTURETYPE_FLATFRAME);
	m_biasFrames = new StackingParameters(this, PICTURETYPE_OFFSETFRAME);

	resultTab = ui->tabWidget->addTab(m_resultParameters, m_resultParameters->windowTitle());
	cometTab = ui->tabWidget->addTab(m_cometStacking, m_cometStacking->windowTitle());;
	lightTab = ui->tabWidget->addTab(m_lightFrames, tr("Light", "IDS_TYPE_LIGHT"));;
	darkTab = ui->tabWidget->addTab(m_darkFrames, tr("Dark", "IDS_TYPE_DARK"));;
	flatTab = ui->tabWidget->addTab(m_flatFrames, tr("Flat", "IDS_TYPE_FLAT"));;
	biasTab = ui->tabWidget->addTab(m_biasFrames, tr("Bias/Offset", "IDS_TYPE_OFFSET"));
	alignmentTab = ui->tabWidget->addTab(m_alignmentParameters, m_alignmentParameters->windowTitle());
	intermediateTab = ui->tabWidget->addTab(m_intermediateFiles, m_intermediateFiles->windowTitle());
	postCalibrationTab = ui->tabWidget->addTab(m_postCalibration, m_postCalibration->windowTitle());;
	outputTab = ui->tabWidget->addTab(m_outputTab, m_outputTab->windowTitle());

	ui->tabWidget->setTabEnabled(resultTab, false);
	ui->tabWidget->setTabEnabled(cometTab, false);
	ui->tabWidget->setTabEnabled(darkTab, false);
	ui->tabWidget->setTabEnabled(flatTab, false);
	ui->tabWidget->setTabEnabled(biasTab, false);
	ui->tabWidget->setTabEnabled(alignmentTab, false);
	ui->tabWidget->setTabEnabled(intermediateTab, false);
	ui->tabWidget->setTabEnabled(postCalibrationTab, false);
	ui->tabWidget->setTabEnabled(outputTab, false);

	//
	// If the user selects a tab we want to know.
	//
	connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

	CWorkspace workspace;
	workspace.Push();
}

StackSettings::~StackSettings()
{
    delete ui;
}

void StackSettings::onInitDialog()
{
	QSettings settings;

	//
	// Restore Window position etc..
	//
	QByteArray ba = settings.value("Dialogs/StackSettings/geometry").toByteArray();
	if (!ba.isEmpty())
	{
		restoreGeometry(ba);
	}
	else
	{
		//
		// Get NATIVE windows ultimate parent
		//
		HWND hParent = GetDeepStackerDlg(nullptr)->m_hWnd;
		RECT r;
		GetWindowRect(hParent, &r);

		QSize size = this->size();

		int top = ((r.top + (r.bottom - r.top) / 2) - (size.height() / 2));
		int left = ((r.left + (r.right - r.left) / 2) - (size.width() / 2));
		move(left, top);
	}

	//
	// Get number of processors we're allowed to use.   Normally this is the number of
	// real cores available, but this can artificially be limited by setting  
	// "MaxProcessors" to 1 in the application settings (registry or ini file).
	//
	// If this is done the the number used will be min("MaxProcessors", cores)
	//
	ui->reducePriority->setChecked(CMultitask::GetReducedThreadsPriority());

	if (CMultitask::GetNrProcessors(true) > 1)
	{
		ui->useAllProcessors->setChecked(CMultitask::GetNrProcessors() > 1);
	}
	else
		ui->useAllProcessors->setDisabled(true);

	// Check if we're allowed to use SIMD vectorized code.
	const bool cpuSupportsAvx2 = AvxSupport::checkAvx2CpuSupport();
	ui->useSimd->setChecked(cpuSupportsAvx2 && CMultitask::GetUseSimd());
	ui->useSimd->setDisabled(!cpuSupportsAvx2);

	//
	// Get the temporary files folder
	//
	QString folder(CAllStackingTasks::GetTemporaryFilesFolder());
	ui->tempFilesFolder->setText(folder);

	if (-1 != startingTab)
	{
		ui->tabWidget->setCurrentIndex(startingTab);
		emit tabChanged(startingTab);
	}
	else if (ui->tabWidget->isTabEnabled(resultTab) && resultTab != ui->tabWidget->currentIndex())
	{
		ui->tabWidget->setCurrentIndex(resultTab);
		emit tabChanged(resultTab);
	}

	updateControls();
}

void StackSettings::updateControls()
{
	if (registeringOnly)
	{
		if (enableDark || enableAll)
			ui->tabWidget->setTabEnabled(darkTab, true);
		if (enableFlat || enableAll)
			ui->tabWidget->setTabEnabled(flatTab, true);
		if (enableBias || enableAll)
			ui->tabWidget->setTabEnabled(biasTab, true);
		ui->tabWidget->setTabEnabled(intermediateTab, false);
	}
	else
	{
		ui->tabWidget->setTabEnabled(resultTab, true);
		if (cometStacking || enableAll)
			ui->tabWidget->setTabEnabled(cometTab, true);
		if (enableDark || enableAll)
			ui->tabWidget->setTabEnabled(darkTab, true);
		if (enableFlat || enableAll)
			ui->tabWidget->setTabEnabled(flatTab, true);
		if (enableBias || enableAll)
			ui->tabWidget->setTabEnabled(biasTab, true);
		ui->tabWidget->setTabEnabled(alignmentTab, true);
		ui->tabWidget->setTabEnabled(intermediateTab, true);
		ui->tabWidget->setTabEnabled(postCalibrationTab, true);
		ui->tabWidget->setTabEnabled(outputTab, true);
	};
}

void StackSettings::tabChanged(int tab)
{
	QWidget * which = ui->tabWidget->widget(tab);
	if (nullptr != which)
		QMetaObject::invokeMethod(which, "onSetActive");
}

void StackSettings::on_chooseFolder_clicked(bool value)
{
	value;
	QString dir =
		QFileDialog::getExistingDirectory(this, tr("Select Temporary Files Folder", "IDS_RECAP_SELECTTEMPFOLDER"),
		ui->tempFilesFolder->text(),
		QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);

	if (dir.length() > 0)
	{
		ui->tempFilesFolder->setText(dir);
	}

}
void StackSettings::accept()
{
	QSettings settings;
	CWorkspace workspace;

	settings.setValue("Dialogs/StackSettings/geometry", saveGeometry());

	//
	// Pop the preserved workspace setting and discard the saved values
	//
	workspace.Pop(false);

	//
	// Harden the workspace changes
	//
	workspace.saveSettings();

	// Save whether allowed to use all processors ane whether to run threads
	// at reduced priority
	if (CMultitask::GetNrProcessors(true) > 1)
		CMultitask::SetUseAllProcessors(ui->useAllProcessors->isChecked());
	CMultitask::SetReducedThreadsPriority(ui->reducePriority->isChecked());

	// Save wether to use SIMD vectorized code.
	CMultitask::SetUseSimd(ui->useSimd->isChecked());

	//
	// Save the temporary files folder
	//
	CAllStackingTasks::SetTemporaryFilesFolder(ui->tempFilesFolder->text());

	//
	// Ask the output tab to save the Output Settings direct to QSettings
	//
	m_outputTab->saveOutputSettings();

	Inherited::accept();
}

void StackSettings::reject()
{
	QSettings settings;
	CWorkspace workspace;

	settings.setValue("Dialogs/StackSettings/geometry", saveGeometry());

	//
	// Pop the preserved workspace setting and restore the status quo ante 
	//
	workspace.Pop(true);

	Inherited::reject();
}

void StackSettings::showEvent(QShowEvent *event)
{
	if (!event->spontaneous())
	{
		if (!initialised)
		{
			initialised = true;
			onInitDialog();
		}
	}
	// Invoke base class showEvent()
	return Inherited::showEvent(event);
}

void StackSettings::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
	}
	Inherited::changeEvent(event);
}
