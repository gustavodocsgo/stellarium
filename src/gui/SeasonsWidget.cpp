#include "SeasonsWidget.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocaleMgr.hpp"
#include "SpecificTimeMgr.hpp"

#include <QCursor>
#include <QToolTip>
#include <QSettings>

SeasonsWidget::SeasonsWidget(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui_seasonsWidget)
{
}

void SeasonsWidget::setup()
{
	ui->setupUi(this);
	connect(&StelApp::getInstance(), &StelApp::languageChanged, this, &SeasonsWidget::retranslate);
	populate();
}

void SeasonsWidget::retranslate()
{
	ui->retranslateUi(this);	
}

void SeasonsWidget::populate()
{
	const auto conf = StelApp::getInstance().getSettings();
	const auto propMgr = StelApp::getInstance().getStelPropertyManager();
}
