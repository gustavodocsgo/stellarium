#include "SeasonsWidget.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelUtils.hpp"
#include "SpecificTimeMgr.hpp"
#include "StelLocaleMgr.hpp"

#include <QPushButton>
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

	core = StelApp::getInstance().getCore();
	specMgr = GETSTELMODULE(SpecificTimeMgr);
	localeMgr = &StelApp::getInstance().getLocaleMgr();

	connect(core, SIGNAL(locationChanged(StelLocation)), this, SLOT(setSeasonLabels()));
	connect(core, SIGNAL(dateChangedByYear()), this, SLOT(setSeasonTimes()));
	connect(specMgr, SIGNAL(eventYearChanged()), this, SLOT(setSeasonTimes()));

	connect(ui->buttonMarchEquinoxCurrent, &QPushButton::clicked, this, [=](){specMgr->currentMarchEquinox();});
	connect(ui->buttonMarchEquinoxNext, &QPushButton::clicked, this, [=](){specMgr->nextMarchEquinox();});
	connect(ui->buttonMarchEquinoxPrevious, &QPushButton::clicked, this, [=](){specMgr->previousMarchEquinox();});

	connect(ui->buttonSeptemberEquinoxCurrent, &QPushButton::clicked, this, [=](){specMgr->currentSeptemberEquinox();});
	connect(ui->buttonSeptemberEquinoxNext, &QPushButton::clicked, this, [=](){specMgr->nextSeptemberEquinox();});
	connect(ui->buttonSeptemberEquinoxPrevious, &QPushButton::clicked, this, [=](){specMgr->previousSeptemberEquinox();});

	connect(ui->buttonJuneSolsticeCurrent, &QPushButton::clicked, this, [=](){specMgr->currentJuneSolstice();});
	connect(ui->buttonJuneSolsticeNext, &QPushButton::clicked, this, [=](){specMgr->nextJuneSolstice();});
	connect(ui->buttonJuneSolsticePrevious, &QPushButton::clicked, this, [=](){specMgr->previousJuneSolstice();});

	connect(ui->buttonDecemberSolsticeCurrent, &QPushButton::clicked, this, [=](){specMgr->currentDecemberSolstice();});
	connect(ui->buttonDecemberSolsticeNext, &QPushButton::clicked, this, [=](){specMgr->nextDecemberSolstice();});
	connect(ui->buttonDecemberSolsticePrevious, &QPushButton::clicked, this, [=](){specMgr->previousDecemberSolstice();});

	populate();

	QSize button = QSize(24, 24);
	ui->buttonMarchEquinoxCurrent->setFixedSize(button);
	ui->buttonMarchEquinoxNext->setFixedSize(button);
	ui->buttonMarchEquinoxPrevious->setFixedSize(button);
	ui->buttonJuneSolsticeCurrent->setFixedSize(button);
	ui->buttonJuneSolsticeNext->setFixedSize(button);
	ui->buttonJuneSolsticePrevious->setFixedSize(button);
	ui->buttonSeptemberEquinoxCurrent->setFixedSize(button);
	ui->buttonSeptemberEquinoxNext->setFixedSize(button);
	ui->buttonSeptemberEquinoxPrevious->setFixedSize(button);
	ui->buttonDecemberSolsticeCurrent->setFixedSize(button);
	ui->buttonDecemberSolsticeNext->setFixedSize(button);
	ui->buttonDecemberSolsticePrevious->setFixedSize(button);
}

void SeasonsWidget::retranslate()
{
	ui->retranslateUi(this);
}

void SeasonsWidget::populate()
{
	// Set season labels
	setSeasonLabels();
	// Compute equinoxes/solstices and fill the time
	setSeasonTimes();
}

void SeasonsWidget::setSeasonLabels()
{
	const double latitide = StelApp::getInstance().getCore()->getCurrentLocation().latitude;
	if (latitide >= 0.)
	{
		// Northern Hemisphere
		ui->labelMarchEquinox->setText(q_("Spring"));
		ui->labelJuneSolstice->setText(q_("Summer"));
		ui->labelSeptemberEquinox->setText(q_("Fall"));
		ui->labelDecemberSolstice->setText(q_("Winter"));
	}
	else
	{
		// Southern Hemisphere
		ui->labelMarchEquinox->setText(q_("Fall"));
		ui->labelJuneSolstice->setText(q_("Winter"));
		ui->labelSeptemberEquinox->setText(q_("Spring"));
		ui->labelDecemberSolstice->setText(q_("Summer"));
	}
}

void SeasonsWidget::setSeasonTimes()
{
	const double JD = core->getJD() + core->getUTCOffset(core->getJD()) / 24;
	int year, month, day;
	double jdFirstDay, jdLastDay;
	StelUtils::getDateFromJulianDay(JD, &year, &month, &day);
	StelUtils::getJDFromDate(&jdFirstDay, year, 1, 1, 0, 0, 1);
	StelUtils::getJDFromDate(&jdLastDay, year, 12, 31, 23, 59, 59);
	const double marchEquinox = specMgr->getEquinox(year, SpecificTimeMgr::Equinox::March);
	const double septemberEquinox = specMgr->getEquinox(year, SpecificTimeMgr::Equinox::September);
	const double juneSolstice = specMgr->getSolstice(year, SpecificTimeMgr::Solstice::June);
	const double decemberSolstice = specMgr->getSolstice(year, SpecificTimeMgr::Solstice::December);
	QString days = qc_("days", "duration");

	// Current year
	ui->labelCurrentYear->setText(QString::number(year));
	// Spring/Fall
	ui->labelMarchEquinoxJD->setText(QString::number(marchEquinox, 'f', 4));
	ui->labelMarchEquinoxLT->setText(QString("%1 %2").arg(localeMgr->getPrintableDateLocal(marchEquinox), localeMgr->getPrintableTimeLocal(marchEquinox)));
	ui->labelMarchEquinoxDuration->setText(QString("%1 %2").arg(QString::number(juneSolstice-marchEquinox, 'f', 2), days));
	// Summer/Winter
	ui->labelJuneSolsticeJD->setText(QString::number(juneSolstice, 'f', 4));
	ui->labelJuneSolsticeLT->setText(QString("%1 %2").arg(localeMgr->getPrintableDateLocal(juneSolstice), localeMgr->getPrintableTimeLocal(juneSolstice)));
	ui->labelJuneSolsticeDuration->setText(QString("%1 %2").arg(QString::number(septemberEquinox-juneSolstice, 'f', 2), days));
	// Fall/Spring
	ui->labelSeptemberEquinoxJD->setText(QString::number(septemberEquinox, 'f', 4));
	ui->labelSeptemberEquinoxLT->setText(QString("%1 %2").arg(localeMgr->getPrintableDateLocal(septemberEquinox), localeMgr->getPrintableTimeLocal(septemberEquinox)));
	ui->labelSeptemberEquinoxDuration->setText(QString("%1 %2").arg(QString::number(decemberSolstice-septemberEquinox, 'f', 2), days));
	// Winter/Summer
	ui->labelDecemberSolsticeJD->setText(QString::number(decemberSolstice, 'f', 4));
	ui->labelDecemberSolsticeLT->setText(QString("%1 %2").arg(localeMgr->getPrintableDateLocal(decemberSolstice), localeMgr->getPrintableTimeLocal(decemberSolstice)));
	const double duration = (marchEquinox-jdFirstDay) + (jdLastDay-decemberSolstice);
	ui->labelDecemberSolsticeDuration->setText(QString("%1 %2").arg(QString::number(duration, 'f', 2), days));
}
