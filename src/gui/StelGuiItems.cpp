/*
 * Stellarium
 * Copyright (C) 2008 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "StelApp.hpp"
#include "StelCore.hpp"

#include "StelUtils.hpp"
#include "SolarSystem.hpp"
#include "StelGuiItems.hpp"
#include "StelGui.hpp"
#include "StelLocaleMgr.hpp"
#include "StelLocation.hpp"
#include "StelMainView.hpp"
#include "StelMovementMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelActionMgr.hpp"
#include "StelProgressController.hpp"
#include "StelPropertyMgr.hpp"
#include "StelObserver.hpp"
#include "SkyGui.hpp"
#include "EphemWrapper.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QRectF>
#include <QDebug>
#include <QScreen>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QTimeLine>
#include <QMouseEvent>
#include <QPixmapCache>
#include <QProgressBar>
#include <QGraphicsWidget>
#include <QGraphicsProxyWidget>
#include <QGraphicsLinearLayout>
#include <QSettings>
#include <QGuiApplication>

// Inspired by text-use-opengl-buffer branch: work around font problems in GUI buttons.
// May be useful in other broken OpenGL font situations. RasPi necessity as of 2016-03-26. Mesa 13 (2016-11) has finally fixed this on RasPi(VC4).
QPixmap getTextPixmap(const QString& str, QFont font)
{
	// Render the text str into a QPixmap.
	QRect strRect = QFontMetrics(font).boundingRect(str);
	int w = strRect.width()+1+static_cast<int>(0.02f*static_cast<float>(strRect.width()));
	int h = strRect.height();

	QPixmap strPixmap(w, h);
	strPixmap.fill(Qt::transparent);
	QPainter painter(&strPixmap);
	font.setStyleStrategy(QFont::NoAntialias); // else: font problems on RasPi20160326
	painter.setFont(font);
	//painter.setRenderHints(QPainter::TextAntialiasing);
	painter.setPen(Qt::white);
	painter.drawText(-strRect.x(), -strRect.y(), str);
	return strPixmap;
}

void StelButton::initCtor(const QPixmap& apixOn,
						  const QPixmap& apixOff,
						  const QPixmap& apixNoChange,
						  const QPixmap& apixHover,
						  StelAction* anAction,
						  StelAction* otherAction,
						  bool noBackground,
						  bool isTristate)
{
	pixOn = apixOn;
	pixOff = apixOff;
	pixHover = apixHover;
	pixNoChange = apixNoChange;

	if(!pixmapsScale)
	{
		pixmapsScale = StelApp::getInstance().getSettings()->value("gui/pixmaps_scale", GUI_INPUT_PIXMAPS_SCALE).toDouble();
	}
	if(pixmapsScale != GUI_INPUT_PIXMAPS_SCALE)
	{
		const auto scale = pixmapsScale/GUI_INPUT_PIXMAPS_SCALE;
		pixOn = pixOn.scaled(pixOn.size()*scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		pixOff = pixOff.scaled(pixOff.size()*scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if(!pixHover.isNull())
			pixHover = pixHover.scaled(pixHover.size()*scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if(!pixNoChange.isNull())
			pixNoChange = pixNoChange.scaled(pixNoChange.size()*scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	pixOn.setDevicePixelRatio(pixmapsScale);
	pixOff.setDevicePixelRatio(pixmapsScale);
	pixHover.setDevicePixelRatio(pixmapsScale);
	pixNoChange.setDevicePixelRatio(pixmapsScale);

	noBckground = noBackground;
	isTristate_ = isTristate;
	opacity = 1.;
	hoverOpacity = 0.;
	action = anAction;
	secondAction = otherAction;
	checked = false;
	flagChangeFocus = false;

	//Q_ASSERT(!pixOn.isNull());
	///Q_ASSERT(!pixOff.isNull());

	if (isTristate_)
	{
		Q_ASSERT(!pixNoChange.isNull());
	}

	setShapeMode(QGraphicsPixmapItem::BoundingRectShape);	
	setAcceptHoverEvents(true);
	timeLine = new QTimeLine(250, this);
	timeLine->setEasingCurve(QEasingCurve(QEasingCurve::OutCurve));
	connect(timeLine, SIGNAL(valueChanged(qreal)),
	        this, SLOT(animValueChanged(qreal)));
	connect(&StelMainView::getInstance(), SIGNAL(updateIconsRequested()), this, SLOT(updateIcon()));  // Not sure if this is ever called?
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	connect(gui, SIGNAL(flagUseButtonsBackgroundChanged(bool)), this, SLOT(updateIcon()));

	if (action!=Q_NULLPTR)
	{
		if (action->isCheckable())
		{
			setChecked(action->isChecked());
			connect(action, SIGNAL(toggled(bool)), this, SLOT(setChecked(bool)));
			connect(this, SIGNAL(toggled(bool)), action, SLOT(setChecked(bool)));
		}
		else
		{
			QObject::connect(this, SIGNAL(triggered()), action, SLOT(trigger()));
		}
	}
	if (secondAction!=Q_NULLPTR)
	{
			QObject::connect(this, SIGNAL(triggeredRight()), secondAction, SLOT(trigger()));
	}
	else {
		setAcceptedMouseButtons(Qt::LeftButton);
	}
}

StelButton::StelButton(QGraphicsItem* parent,
					   const QPixmap& pixOn,
					   const QPixmap& pixOff,
					   const QPixmap& pixHover,
					   StelAction *action,
					   bool noBackground,
					   StelAction *otherAction)
	: QGraphicsPixmapItem(pixOff, parent)
{
	initCtor(pixOn, pixOff, QPixmap(), pixHover, action, otherAction, noBackground, false);
}

StelButton::StelButton(QGraphicsItem* parent,
					   const QPixmap& pixOn,
					   const QPixmap& pixOff,
					   const QPixmap& pixNoChange,
					   const QPixmap& pixHover,
					   const QString& actionId,
					   bool noBackground,
					   bool isTristate)
	: QGraphicsPixmapItem(pixOff, parent)
{
	StelAction *action = StelApp::getInstance().getStelActionManager()->findAction(actionId);
	initCtor(pixOn, pixOff, pixNoChange, pixHover, action, Q_NULLPTR, noBackground, isTristate);
}

StelButton::StelButton(QGraphicsItem* parent,
					   const QPixmap& pixOn,
					   const QPixmap& pixOff,
					   const QPixmap& pixHover,
					   const QString& actionId,
					   bool noBackground,
					   const QString &otherActionId)
	: QGraphicsPixmapItem(pixOff, parent)
{
	StelAction *action = StelApp::getInstance().getStelActionManager()->findAction(actionId);
	StelAction *otherAction=Q_NULLPTR;
	if (otherActionId.length()>0)
		otherAction = StelApp::getInstance().getStelActionManager()->findAction(otherActionId);

	initCtor(pixOn, pixOff, QPixmap(), pixHover, action, otherAction, noBackground, false);
}


int StelButton::toggleChecked(int checked)
{
	if (!isTristate_)
		checked = !!!checked;
	else
	{
		if (++checked > ButtonStateNoChange)
			checked = ButtonStateOff;
	}
	return checked;
}

void StelButton::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
	timeLine->setDirection(QTimeLine::Forward);
	if (timeLine->state()!=QTimeLine::Running)
		timeLine->start();

	emit hoverChanged(true);
}

void StelButton::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
	timeLine->setDirection(QTimeLine::Backward);
	if (timeLine->state()!=QTimeLine::Running)
		timeLine->start();
	emit hoverChanged(false);
}

void StelButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button()==Qt::LeftButton)
	{
		QGraphicsItem::mousePressEvent(event);
		event->accept();
		setChecked(toggleChecked(checked));
		if (!triggerOnRelease)
		{
			emit toggled(checked);
			emit triggered();
		}
	}
	else if  (event->button()==Qt::RightButton)
	{
		QGraphicsItem::mousePressEvent(event);
		event->accept();
		//setChecked(toggleChecked(checked));
		if (!triggerOnRelease)
		{
			//emit toggled(checked);
			emit triggeredRight();
		}
	}
}

void StelButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button()==Qt::LeftButton)
	{
		if (action!=Q_NULLPTR && !action->isCheckable())
			setChecked(toggleChecked(checked));

		if (flagChangeFocus) // true if button is on bottom bar
			StelMainView::getInstance().focusSky(); // Change the focus after clicking on button
		if (triggerOnRelease)
		{
			emit toggled(checked);
			emit triggered();
		}
	}
	else if  (event->button()==Qt::RightButton)
	{
		//if (flagChangeFocus) // true if button is on bottom bar
		//	StelMainView::getInstance().focusSky(); // Change the focus after clicking on button
		if (triggerOnRelease)
		{
			//emit toggled(checked);
			emit triggeredRight();
		}
	}
}

void StelButton::updateIcon()
{
	if (opacity < 0.)
		opacity = 0;
	QPixmap pix(pixOn.size());
	pix.setDevicePixelRatio(pixmapsScale);
	pix.fill(QColor(0,0,0,0));
	QPainter painter(&pix);
	painter.setOpacity(opacity);
	if (!pixBackground.isNull() && noBckground==false && StelApp::getInstance().getStelPropertyManager()->getStelPropertyValue("StelGui.flagUseButtonsBackground").toBool())
		painter.drawPixmap(0, 0, pixBackground);

	painter.drawPixmap(0, 0,
		(isTristate_ && checked == ButtonStateNoChange) ? (pixNoChange) :
		(checked == ButtonStateOn) ? (pixOn) :
		/* (checked == ButtonStateOff) ? */ (pixOff));

	if (hoverOpacity > 0)
	{
		painter.setOpacity(hoverOpacity * opacity);
		painter.drawPixmap(0, 0, pixHover);
	}
	setPixmap(pix);
	scaledCurrentPixmap = {};
}

void StelButton::animValueChanged(qreal value)
{
	hoverOpacity = value;
	updateIcon();
}

void StelButton::setChecked(int b)
{
	checked=b;
	updateIcon();
}

void StelButton::setBackgroundPixmap(const QPixmap &newBackground)
{
	pixBackground = newBackground;
	updateIcon();
}

QRectF StelButton::boundingRect() const
{
	return QRectF(0,0, getButtonPixmapWidth(), getButtonPixmapHeight());
}

void StelButton::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	/* QPixmap::scaled has much better quality than that scaling via QPainter::drawPixmap, so let's
	 * have our scaled copy of the pixmap.
	 * NOTE: we cache this copy for two reasons:
	 * 1. Performance
	 * 2. Work around a Qt problem (I think it's a bug): when rendering multiple StelButton items
	 *    in sequence, only the first one gets the necessary texture parameters set, particularly
	 *    GL_TEXTURE_MIN_FILTER. On deletion of QPixmap the texture is deleted, and its Id gets
	 *    assigned to the next QPixmap. Apparently, the Id gets cached somewhere in Qt internals, and
	 *    becomes similar to a dangling pointer, informing Qt as if the necessary setup has already
	 *    been done. The result is that after the first button all others in the same panel are black
	 *    rectangles.
	 *    Our keeping QPixmap alive instead of deleting it on return from this function prevents this.
	 */
	const double ratio = QOpenGLContext::currentContext()->screen()->devicePixelRatio();
	if(scaledCurrentPixmap.isNull() || ratio != scaledCurrentPixmap.devicePixelRatioF())
	{
		const auto scale = ratio / pixmapsScale;
		scaledCurrentPixmap = pixmap().scaled(pixOn.size()*scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		scaledCurrentPixmap.setDevicePixelRatio(ratio);
	}
	// Align the pixmap to pixel grid, otherwise we'll get artifacts at some scaling factors.
	const auto transform = painter->combinedTransform();
	const auto shift = QPointF(-std::fmod(transform.dx(), 1.),
							   -std::fmod(transform.dy(), 1.));
	painter->drawPixmap(shift/ratio, scaledCurrentPixmap);
}

LeftStelBar::LeftStelBar(QGraphicsItem* parent)
	: QGraphicsItem(parent)
	, hideTimeLine(Q_NULLPTR)
	, helpLabelPixmap(Q_NULLPTR)
{
	// Create the help label
	helpLabel = new QGraphicsSimpleTextItem("", this);
	helpLabel->setBrush(QBrush(QColor::fromRgbF(1,1,1,1)));
	if (qApp->property("text_texture")==true) // CLI option -t given?
		helpLabelPixmap=new QGraphicsPixmapItem(this);
}

LeftStelBar::~LeftStelBar()
{
	if (helpLabelPixmap) { delete helpLabelPixmap; helpLabelPixmap=Q_NULLPTR; }
}

void LeftStelBar::addButton(StelButton* button)
{
	double posY = 0;
	if (QGraphicsItem::childItems().size()!=0)
	{
		const QRectF& r = childrenBoundingRect();
		posY += r.bottom()-1;
	}
	button->setParentItem(this);
	button->setFocusOnSky(false);
	button->prepareGeometryChange(); // could possibly be removed when qt 4.6 become stable
	button->setPos(0., qRound(posY+10.5));

	connect(button, SIGNAL(hoverChanged(bool)), this, SLOT(buttonHoverChanged(bool)));
}

void LeftStelBar::paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*)
{
}

QRectF LeftStelBar::boundingRect() const
{
	return childrenBoundingRect();
}

QRectF LeftStelBar::boundingRectNoHelpLabel() const
{
	// Re-use original Qt code, just remove the help label
	QRectF childRect;
	for (auto* child : QGraphicsItem::childItems())
	{
		if ((child==helpLabel) || (child==helpLabelPixmap))
			continue;
		QPointF childPos = child->pos();
		QTransform matrix = child->transform() * QTransform().translate(childPos.x(), childPos.y());
		childRect |= matrix.mapRect(child->boundingRect() | child->childrenBoundingRect());
	}
	return childRect;
}


// Update the help label when a button is hovered
void LeftStelBar::buttonHoverChanged(bool b)
{
	StelButton* button = qobject_cast<StelButton*>(sender());
	Q_ASSERT(button);
	if (b==true)
	{
		if (button->action)
		{
			QString tip(button->action->getText());
			QString shortcut(button->action->getShortcut().toString(QKeySequence::NativeText));
			if (!shortcut.isEmpty())
			{
				//XXX: this should be unnecessary since we used NativeText.
				if (shortcut == "Space")
					shortcut = q_("Space");
				tip += "  [" + shortcut + "]";
			}
			helpLabel->setText(tip);
			helpLabel->setPos(qRound(boundingRectNoHelpLabel().width()+15.5),qRound(button->pos().y()+button->getButtonPixmapHeight()/2-8));
			if (qApp->property("text_texture")==true)
			{
				helpLabel->setVisible(false);
				helpLabelPixmap->setPixmap(getTextPixmap(tip, helpLabel->font()));
				helpLabelPixmap->setPos(helpLabel->pos());
				helpLabelPixmap->setVisible(true);
			}
		}
	}
	else
	{
		helpLabel->setText("");
		if (qApp->property("text_texture")==true)
			helpLabelPixmap->setVisible(false);
	}
	// Update the screen as soon as possible.
	StelMainView::getInstance().thereWasAnEvent();
}

// Set the pen for all the sub elements
void LeftStelBar::setColor(const QColor& c)
{
	helpLabel->setBrush(c);
}

BottomStelBar::BottomStelBar(QGraphicsItem* parent,
                             const QPixmap& pixLeft,
                             const QPixmap& pixRight,
                             const QPixmap& pixMiddle,
                             const QPixmap& pixSingle) :
	QGraphicsItem(parent),
	locationPixmap(Q_NULLPTR),
	datetimePixmap(Q_NULLPTR),
	fovPixmap(Q_NULLPTR),
	fpsPixmap(Q_NULLPTR),
	pixBackgroundLeft(pixLeft),
	pixBackgroundRight(pixRight),
	pixBackgroundMiddle(pixMiddle),
	pixBackgroundSingle(pixSingle),
	helpLabelPixmap(Q_NULLPTR)
{
	// The text is dummy just for testing
	datetime = new QGraphicsSimpleTextItem("2008-02-06  17:33", this);
	location = new QGraphicsSimpleTextItem("Munich, Earth, 500m", this);
	fov = new QGraphicsSimpleTextItem("FOV 43.45", this);
	fps = new QGraphicsSimpleTextItem("43.2 FPS", this);
	if (qApp->property("text_texture")==true) // CLI option -t given?
	{
		datetimePixmap=new QGraphicsPixmapItem(this);
		locationPixmap=new QGraphicsPixmapItem(this);
		fovPixmap=new QGraphicsPixmapItem(this);
		fpsPixmap=new QGraphicsPixmapItem(this);
		helpLabelPixmap=new QGraphicsPixmapItem(this);
	}

	// Create the help label
	helpLabel = new QGraphicsSimpleTextItem("", this);
	helpLabel->setBrush(QBrush(QColor::fromRgbF(1,1,1,1)));

	setColor(QColor::fromRgbF(1,1,1,1));

	setFontSizeFromApp(StelApp::getInstance().getScreenFontSize());
	connect(&StelApp::getInstance(), SIGNAL(screenFontSizeChanged(int)), this, SLOT(setFontSizeFromApp(int)));
	connect(&StelApp::getInstance(), SIGNAL(fontChanged(QFont)), this, SLOT(setFont(QFont)));

	QSettings* confSettings = StelApp::getInstance().getSettings();
	setFlagShowTime(confSettings->value("gui/flag_show_datetime", true).toBool());
	setFlagShowLocation(confSettings->value("gui/flag_show_location", true).toBool());
	setFlagShowFov(confSettings->value("gui/flag_show_fov", true).toBool());
	setFlagShowFps(confSettings->value("gui/flag_show_fps", true).toBool());
	setFlagTimeJd(confSettings->value("gui/flag_time_jd", false).toBool());
	setFlagFovDms(confSettings->value("gui/flag_fov_dms", false).toBool());
	setFlagShowTz(confSettings->value("gui/flag_show_tz", true).toBool());
}

//! connect from StelApp to resize fonts on the fly.
void BottomStelBar::setFontSizeFromApp(int size)
{
	// Font size was developed based on base font size 13, i.e. 12
	int screenFontSize = size-1;
	QFont font=QGuiApplication::font();
	font.setPixelSize(screenFontSize);
	datetime->setFont(font);
	location->setFont(font);
	fov->setFont(font);
	fps->setFont(font);
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui)
	{
		// to avoid crash
		SkyGui* skyGui=gui->getSkyGui();
		if (skyGui)
			skyGui->updateBarsPos();
	}
}

//! connect from StelApp to resize fonts on the fly.
void BottomStelBar::setFont(QFont font)
{
	font.setPixelSize(StelApp::getInstance().getScreenFontSize()-1);
	datetime->setFont(font);
	location->setFont(font);
	fov->setFont(font);
	fps->setFont(font);
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui)
	{
		// to avoid crash
		SkyGui* skyGui=gui->getSkyGui();
		if (skyGui)
			skyGui->updateBarsPos();
	}
}

BottomStelBar::~BottomStelBar()
{
	// Remove currently hidden buttons which are not deleted by a parent element
	for (auto& group : buttonGroups)
	{
		for (auto* b : qAsConst(group.elems))
		{
			if (b->parentItem()==Q_NULLPTR)
			{
				delete b;
				b=Q_NULLPTR;
			}
		}
	}
	if (datetimePixmap) { delete datetimePixmap; datetimePixmap=Q_NULLPTR; }
	if (locationPixmap) { delete locationPixmap; locationPixmap=Q_NULLPTR; }
	if (fovPixmap) { delete fovPixmap; fovPixmap=Q_NULLPTR; }
	if (fpsPixmap) { delete fpsPixmap; fpsPixmap=Q_NULLPTR; }
	if (helpLabelPixmap) { delete helpLabelPixmap; helpLabelPixmap=Q_NULLPTR; }
}

void BottomStelBar::addButton(StelButton* button, const QString& groupName, const QString& beforeActionName)
{
	QList<StelButton*>& g = buttonGroups[groupName].elems;
	bool done = false;
	for (int i=0; i<g.size(); ++i)
	{
		if (g[i]->action && g[i]->action->objectName()==beforeActionName)
		{
			g.insert(i, button);
			done = true;
			break;
		}
	}
	if (done == false)
		g.append(button);

	button->setVisible(true);
	button->setParentItem(this);
	button->setFocusOnSky(true);
	updateButtonsGroups();

	connect(button, SIGNAL(hoverChanged(bool)), this, SLOT(buttonHoverChanged(bool)));
	emit sizeChanged();
}

StelButton* BottomStelBar::hideButton(const QString& actionName)
{
	QString gName;
	StelButton* bToRemove = Q_NULLPTR;
	for (auto iter = buttonGroups.begin(); iter != buttonGroups.end(); ++iter)
	{
		int i=0;
		for (auto* b : qAsConst(iter.value().elems))
		{
			if (b->action && b->action->objectName()==actionName)
			{
				gName = iter.key();
				bToRemove = b;
				iter.value().elems.removeAt(i);
				break;
			}
			++i;
		}
	}
	if (bToRemove == Q_NULLPTR)
		return Q_NULLPTR;
	if (buttonGroups[gName].elems.size() == 0)
	{
		buttonGroups.remove(gName);
	}
	// Cannot really delete because some part of the GUI depend on the presence of some buttons
	// so just make invisible
	bToRemove->setParentItem(Q_NULLPTR);
	bToRemove->setVisible(false);
	updateButtonsGroups();
	emit sizeChanged();
	return bToRemove;
}

// Set the margin at the left and right of a button group in pixels
void BottomStelBar::setGroupMargin(const QString& groupName, int left, int right)
{
	if (!buttonGroups.contains(groupName))
		return;
	buttonGroups[groupName].leftMargin = left;
	buttonGroups[groupName].rightMargin = right;
	updateButtonsGroups();
}

//! Change the background of a group
void BottomStelBar::setGroupBackground(const QString& groupName,
                                       const QPixmap& pixLeft,
                                       const QPixmap& pixRight,
                                       const QPixmap& pixMiddle,
                                       const QPixmap& pixSingle)
{
	if (!buttonGroups.contains(groupName))
		return;

	buttonGroups[groupName].pixBackgroundLeft = new QPixmap(pixLeft);
	buttonGroups[groupName].pixBackgroundRight = new QPixmap(pixRight);
	buttonGroups[groupName].pixBackgroundMiddle = new QPixmap(pixMiddle);
	buttonGroups[groupName].pixBackgroundSingle = new QPixmap(pixSingle);
	updateButtonsGroups();
}

QRectF BottomStelBar::getButtonsBoundingRect() const
{
	// Re-use original Qt code, just remove the help label
	QRectF childRect;
	bool hasBtn = false;
	for (auto* child : QGraphicsItem::childItems())
	{
		if (qgraphicsitem_cast<StelButton*>(child)==Q_NULLPTR)
			continue;
		hasBtn = true;
		QPointF childPos = child->pos();
		QTransform matrix = child->transform() * QTransform().translate(childPos.x(), childPos.y());
		childRect |= matrix.mapRect(child->boundingRect() | child->childrenBoundingRect());
	}

	if (hasBtn)
		return QRectF(0, 0, childRect.width()-1, childRect.height()-1);
	else
		return QRectF();
}

void BottomStelBar::updateButtonsGroups()
{
	double x = 0;
	double y = datetime->boundingRect().height() + 3;
	for (auto& group : buttonGroups)
	{
		QList<StelButton*>& buttons = group.elems;
		if (buttons.empty())
			continue;
		x += group.leftMargin;
		int n = 0;
		for (auto* b : buttons)
		{
			// We check if the group has its own background if not the case
			// We apply a default background.
			if (n == 0)
			{
				if (buttons.size() == 1)
				{
					if (group.pixBackgroundSingle == Q_NULLPTR)
						b->setBackgroundPixmap(pixBackgroundSingle);
					else
						b->setBackgroundPixmap(*group.pixBackgroundSingle);
				}
				else
				{
					if (group.pixBackgroundLeft == Q_NULLPTR)
						b->setBackgroundPixmap(pixBackgroundLeft);
					else
						b->setBackgroundPixmap(*group.pixBackgroundLeft);
				}
			}
			else if (n == buttons.size()-1)
			{
				if (buttons.size() != 1)
				{
					if (group.pixBackgroundSingle == Q_NULLPTR)
						b->setBackgroundPixmap(pixBackgroundSingle);
					else
						b->setBackgroundPixmap(*group.pixBackgroundSingle);
				}
				if (group.pixBackgroundRight == Q_NULLPTR)
					b->setBackgroundPixmap(pixBackgroundRight);
				else
					b->setBackgroundPixmap(*group.pixBackgroundRight);
			}
			else
			{
				if (group.pixBackgroundMiddle == Q_NULLPTR)
					b->setBackgroundPixmap(pixBackgroundMiddle);
				else
					b->setBackgroundPixmap(*group.pixBackgroundMiddle);
			}
			// Update the button pixmap
			b->animValueChanged(0.);
			b->setPos(x, y);
			x += b->getButtonPixmapWidth();
			++n;
		}
		x+=group.rightMargin;
	}
	updateText(true);
}

// create text elements and tooltips in bottom toolbar.
// Make sure to avoid any change if not necessary to avoid triggering useless redraw
void BottomStelBar::updateText(bool updatePos)
{
	StelCore* core = StelApp::getInstance().getCore();
	const double jd = core->getJD();
	const double deltaT = core->getDeltaT();
	const double sigma = StelUtils::getDeltaTStandardError(jd);
	QString sigmaInfo = "";
	QString validRangeMarker = "";
	core->getCurrentDeltaTAlgorithmValidRangeDescription(jd, &validRangeMarker);

	const StelLocaleMgr& locmgr = StelApp::getInstance().getLocaleMgr();
	QString tz = locmgr.getPrintableTimeZoneLocal(jd);
	QString newDateInfo = " ";
	if (getFlagShowTime())
	{
		if (getFlagShowTz())
			newDateInfo = QString("%1 %2 %3").arg(locmgr.getPrintableDateLocal(jd), locmgr.getPrintableTimeLocal(jd), tz);
		else
			newDateInfo = QString("%1 %2").arg(locmgr.getPrintableDateLocal(jd), locmgr.getPrintableTimeLocal(jd));
	}
	QString newDateAppx = QString("JD %1").arg(jd, 0, 'f', 5); // up to seconds
	if (getFlagTimeJd())
	{
		newDateAppx = newDateInfo;
		newDateInfo = QString("JD %1").arg(jd, 0, 'f', 5); // up to seconds
	}

	QString planetName = core->getCurrentLocation().planetName;
	QString planetNameI18n;
	if (planetName=="SpaceShip") // Avoid crash
	{
		const StelTranslator& trans = StelApp::getInstance().getLocaleMgr().getSkyTranslator();
		planetNameI18n = trans.qtranslate(planetName, "special celestial body"); // added context
	}
	else
		planetNameI18n = GETSTELMODULE(SolarSystem)->searchByEnglishName(planetName)->getNameI18n();

	QString tzName = core->getCurrentTimeZone();
	if (tzName.contains("system_default") || (tzName.isEmpty() && planetName=="Earth"))
		tzName = q_("System default");

	QString currTZ = QString("%1: %2").arg(q_("Time zone"), tzName);

	if (tzName.contains("LMST") || tzName.contains("auto") || (planetName=="Earth" && jd<=StelCore::TZ_ERA_BEGINNING && !core->getUseCustomTimeZone()) )
		currTZ = q_("Local Mean Solar Time");

	if (tzName.contains("LTST"))
		currTZ = q_("Local True Solar Time");

	// TRANSLATORS: unit of measurement: minutes per second
	QString timeRateMU = qc_("min/s", "unit of measurement");
	double timeRate = qAbs(core->getTimeRate()/StelCore::JD_SECOND);
	double timeSpeed = timeRate/60.;

	if (timeSpeed>=60.)
	{
		timeSpeed /= 60.;
		// TRANSLATORS: unit of measurement: hours per second
		timeRateMU = qc_("hr/s", "unit of measurement");
	}
	if (timeSpeed>=24.)
	{
		timeSpeed /= 24.;
		// TRANSLATORS: unit of measurement: days per second
		timeRateMU = qc_("d/s", "unit of measurement");
	}
	if (timeSpeed>=365.25)
	{
		timeSpeed /= 365.25;
		// TRANSLATORS: unit of measurement: years per second
		timeRateMU = qc_("yr/s", "unit of measurement");
	}
	QString timeRateInfo = QString("%1: x%2").arg(q_("Simulation speed"), QString::number(timeRate, 'f', 0));
	if (timeRate>60.)
		timeRateInfo = QString("%1: x%2 (%3 %4)").arg(q_("Simulation speed"), QString::number(timeRate, 'f', 0), QString::number(timeSpeed, 'f', 2), timeRateMU);

	if (datetime->text()!=newDateInfo)
	{
		updatePos = true;
		datetime->setText(newDateInfo);
	}

	if (core->getCurrentDeltaTAlgorithm()!=StelCore::WithoutCorrection)
	{
		if (sigma>0)
			sigmaInfo = QString("; %1(%2T) = %3s").arg(QChar(0x03c3)).arg(QChar(0x0394)).arg(sigma, 3, 'f', 1);

		QString deltaTInfo = "";
		if (qAbs(deltaT)>60.)
			deltaTInfo = QString("%1 (%2s)%3").arg(StelUtils::hoursToHmsStr(deltaT/3600.)).arg(deltaT, 5, 'f', 2).arg(validRangeMarker);
		else
			deltaTInfo = QString("%1s%2").arg(deltaT, 3, 'f', 3).arg(validRangeMarker);

		// the corrective ndot to be displayed could be set according to the currently used DeltaT algorithm.
		//float ndot=core->getDeltaTnDot();
		// or just to the used ephemeris. This has to be read as "Selected DeltaT formula used, but with the ephemeris's nDot applied it corrects DeltaT to..."
		const double ndot=( (EphemWrapper::use_de430(jd) || EphemWrapper::use_de431(jd) || EphemWrapper::use_de440(jd) || EphemWrapper::use_de441(jd)) ? -25.8 : -23.8946 );

		datetime->setToolTip(QString("<p style='white-space:pre'>%1T = %2 [n%8 @ %3\"/cy%4%5]<br>%6<br>%7<br>%9</p>").arg(QChar(0x0394), deltaTInfo, QString::number(ndot, 'f', 4), QChar(0x00B2), sigmaInfo, newDateAppx, currTZ, QChar(0x2032), timeRateInfo));
	}
	else
		datetime->setToolTip(QString("<p style='white-space:pre'>%1<br>%2<br>%3</p>").arg(newDateAppx, currTZ, timeRateInfo));

	if (qApp->property("text_texture")==true) // CLI option -t given?
	{
		datetime->setVisible(false); // hide normal thingy.
		datetimePixmap->setPixmap(getTextPixmap(newDateInfo, datetime->font()));
	}

	// build location tooltip
	QString newLocation = "";
	if (getFlagShowLocation())
	{
		const StelLocation* loc = &core->getCurrentLocation();
		if (core->getCurrentPlanet()->getPlanetType()==Planet::isObserver)
			newLocation = planetNameI18n;
		else if(loc->name.isEmpty())
			newLocation = planetNameI18n +", "+StelUtils::decDegToDmsStr(loc->getLatitude())+", "+StelUtils::decDegToDmsStr(loc->getLongitude());
		else if (loc->name.contains("->")) // a spaceship
			newLocation = QString("%1 [%2 %3]").arg(planetNameI18n, q_("flight"), loc->name);
		else
			//TRANSLATORS: Unit of measure for distance - meter
			newLocation = planetNameI18n +", "+q_(loc->name) + ", "+ QString("%1 %2").arg(loc->altitude).arg(qc_("m", "distance"));
	}
	// TODO: When topocentric switch is toggled, this must be redrawn!
	if (location->text()!=newLocation)
	{
		updatePos = true;
		location->setText(newLocation);
		double lat = static_cast<double>(core->getCurrentLocation().getLatitude());
		double lon = static_cast<double>(core->getCurrentLocation().getLongitude());
		QString latStr, lonStr, pm;
		if (lat >= 0)
			pm = "N";
		else
		{
			pm = "S";
			lat *= -1;
		}
		latStr = QString("%1%2%3").arg(pm).arg(lat).arg(QChar(0x00B0));
		if (lon >= 0)
			pm = "E";
		else
		{
			pm = "W";
			lon *= -1;
		}
		lonStr = QString("%1%2%3").arg(pm).arg(lon).arg(QChar(0x00B0));
		QString rho, weather;
		if (core->getUseTopocentricCoordinates())
			rho = QString("%1 %2 %3").arg(q_("planetocentric distance")).arg(core->getCurrentObserver()->getDistanceFromCenter() * AU).arg(qc_("km", "distance"));
		else
			rho = q_("planetocentric observer");

		if (newLocation.contains("->")) // a spaceship
			location->setToolTip(QString());
		else
		{
			if (core->getCurrentPlanet()->hasAtmosphere())
			{
				const StelPropertyMgr* propMgr=StelApp::getInstance().getStelPropertyManager();
				weather = QString("%1: %2 %3; %4: %5 °C").arg(q_("Atmospheric pressure"), QString::number(propMgr->getStelPropertyValue("StelSkyDrawer.atmospherePressure").toDouble(), 'f', 2), qc_("mbar", "pressure unit"), q_("temperature"), QString::number(propMgr->getStelPropertyValue("StelSkyDrawer.atmosphereTemperature").toDouble(), 'f', 1));
				location->setToolTip(QString("<p style='white-space:pre'>%1 %2; %3<br>%4</p>").arg(latStr, lonStr, rho, weather));
			}
			else if (core->getCurrentPlanet()->getPlanetType()==Planet::isObserver)
				newLocation = planetNameI18n;
			else
				location->setToolTip(QString("%1 %2; %3").arg(latStr, lonStr, rho));
		}

		if (qApp->property("text_texture")==true) // CLI option -t given?
		{
			locationPixmap->setPixmap(getTextPixmap(newLocation, location->font()));
			location->setVisible(false);
		}
	}

	QString str;

	// build fov tooltip
	QTextStream wos(&str);
	// TRANSLATORS: Field of view. Please use abbreviation.
	QString fovstr = QString("%1 ").arg(qc_("FOV", "abbreviation"));
	QString fovdms = StelUtils::decDegToDmsStr(core->getMovementMgr()->getCurrentFov());
	if (getFlagFovDms())
	{
		wos << fovstr << fovdms;
	}
	else
	{
		wos << fovstr << qSetRealNumberPrecision(3) << core->getMovementMgr()->getCurrentFov() << QChar(0x00B0);
	}

	if (fov->text()!=str)
	{
		updatePos = true;
		if (getFlagShowFov())
		{
			fov->setText(str);
			fov->setToolTip(QString("%1: %2").arg(q_("Field of view"), fovdms));
			if (qApp->property("text_texture")==true) // CLI option -t given?
			{
				fovPixmap->setPixmap(getTextPixmap(str, fov->font()));
				fov->setVisible(false);
			}
		}
		else
		{
			fov->setText("");
			fov->setToolTip("");
		}
	}

	str="";

	// build fps tooltip
	QTextStream wos2(&str);
	// TRANSLATORS: Frames per second. Please use abbreviation.
	QString fpsstr = QString(" %1").arg(qc_("FPS", "abbreviation"));
	wos2 << qSetRealNumberPrecision(3) << StelApp::getInstance().getFps() << fpsstr;
	if (fps->text()!=str)
	{
		updatePos = true;
		if (getFlagShowFps())
		{
			fps->setText(str);
			fps->setToolTip(q_("Frames per second"));
			if (qApp->property("text_texture")==true) // CLI option -t given?
			{
				fpsPixmap->setPixmap(getTextPixmap(str, fps->font()));
				fps->setVisible(false);
			}
		}
		else
		{
			fps->setText("");
			fps->setToolTip("");
		}
	}

	if (updatePos)
	{
		QFontMetrics fpsMetrics(fps->font());
		int fpsShift = fpsMetrics.boundingRect(fpsstr).width() + 50;

		QFontMetrics fovMetrics(fov->font());
		int fovShift = fpsShift + fovMetrics.boundingRect(fovstr).width() + 80;
		if (getFlagFovDms())
			fovShift += 25;

		QRectF rectCh = getButtonsBoundingRect();
		location->setPos(0, 0);		
		int dtp = static_cast<int>(rectCh.right()-datetime->boundingRect().width())-5;
		if ((dtp%2) == 1) dtp--; // make even pixel
		datetime->setPos(dtp,0);
		fov->setPos(datetime->x()-fovShift, 0);
		fps->setPos(datetime->x()-fpsShift, 0);
		if (qApp->property("text_texture")==true) // CLI option -t given?
		{
			locationPixmap->setPos(0,0);
			int dtp = static_cast<int>(rectCh.right()-datetimePixmap->boundingRect().width())-5;
			if ((dtp%2) == 1) dtp--; // make even pixel
			datetimePixmap->setPos(dtp,0);
			fovPixmap->setPos(datetime->x()-fovShift, 0);
			fpsPixmap->setPos(datetime->x()-fpsShift, 0);
		}
	}
}

void BottomStelBar::paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*)
{
	updateText();
}

QRectF BottomStelBar::boundingRect() const
{
	if (QGraphicsItem::childItems().size()==0)
		return QRectF();
	const QRectF& r = childrenBoundingRect();
	return QRectF(0, 0, r.width()-1, r.height()-1);
}

QRectF BottomStelBar::boundingRectNoHelpLabel() const
{
	// Re-use original Qt code, just remove the help label
	QRectF childRect;
	for (const auto* child : QGraphicsItem::childItems())
	{
		if ((child==helpLabel) || (child==helpLabelPixmap))
			continue;
		QPointF childPos = child->pos();
		QTransform matrix = child->transform() * QTransform().translate(childPos.x(), childPos.y());
		childRect |= matrix.mapRect(child->boundingRect() | child->childrenBoundingRect());
	}
	return childRect;
}

// Set the pen for all the sub elements
void BottomStelBar::setColor(const QColor& c)
{
	datetime->setBrush(c);
	location->setBrush(c);
	fov->setBrush(c);
	fps->setBrush(c);
	helpLabel->setBrush(c);
}

// Update the help label when a button is hovered
void BottomStelBar::buttonHoverChanged(bool b)
{
	StelButton* button = qobject_cast<StelButton*>(sender());
	Q_ASSERT(button);
	if (b==true)
	{
		StelAction* action = button->action;
		if (action)
		{
			QString tip(action->getText());
			QString shortcut(action->getShortcut().toString(QKeySequence::NativeText));
			if (!shortcut.isEmpty())
			{
				//XXX: this should be unnecessary since we used NativeText.
				if (shortcut == "Space")
					shortcut = q_("Space");
				tip += "  [" + shortcut + "]";
			}
			helpLabel->setText(tip);
			//helpLabel->setPos(button->pos().x()+button->pixmap().size().width()/2,-27);
			helpLabel->setPos(20,-27);
			if (qApp->property("text_texture")==true)
			{
				helpLabel->setVisible(false);
				helpLabelPixmap->setPixmap(getTextPixmap(tip, helpLabel->font()));
				helpLabelPixmap->setPos(helpLabel->pos());
				helpLabelPixmap->setVisible(true);
			}
		}
	}
	else
	{
		helpLabel->setText("");
		if (qApp->property("text_texture")==true)
			helpLabelPixmap->setVisible(false);
	}
	// Update the screen as soon as possible.
	StelMainView::getInstance().thereWasAnEvent();
}

StelBarsPath::StelBarsPath(QGraphicsItem* parent) : QGraphicsPathItem(parent), roundSize(6)
{
	QPen aPen(QColor::fromRgbF(0.7,0.7,0.7,0.5));
	aPen.setWidthF(1.);
	setBrush(QBrush(QColor::fromRgbF(0.22, 0.22, 0.23, 0.2)));
	setPen(aPen);
}

void StelBarsPath::updatePath(BottomStelBar* bot, LeftStelBar* lef)
{
	QPainterPath newPath;
	QPointF p = lef->pos() + QPointF(-0.5,0.5);
	QRectF r = lef->boundingRectNoHelpLabel();
	QPointF p2 = bot->pos() + QPointF(-0.5,0.5);
	QRectF r2 = bot->boundingRectNoHelpLabel();

	newPath.moveTo(p.x()-roundSize, p.y()-roundSize);
	newPath.lineTo(p.x()+r.width(),p.y()-roundSize);
	newPath.arcTo(p.x()+r.width()-roundSize, p.y()-roundSize, 2.*roundSize, 2.*roundSize, 90, -90);
	newPath.lineTo(p.x()+r.width()+roundSize, p2.y()-roundSize);
	newPath.lineTo(p2.x()+r2.width(),p2.y()-roundSize);
	newPath.arcTo(p2.x()+r2.width()-roundSize, p2.y()-roundSize, 2.*roundSize, 2.*roundSize, 90, -90);
	newPath.lineTo(p2.x()+r2.width()+roundSize, p2.y()+r2.height()+roundSize);
	newPath.lineTo(p.x()-roundSize, p2.y()+r2.height()+roundSize);
	setPath(newPath);
}

void StelBarsPath::setBackgroundOpacity(double opacity)
{
	setBrush(QBrush(QColor::fromRgbF(0.22, 0.22, 0.23, opacity)));
}

StelProgressBarMgr::StelProgressBarMgr(QGraphicsItem* parent):
	QGraphicsWidget(parent)
{
	setLayout(new QGraphicsLinearLayout(Qt::Vertical));
}
/*
QRectF StelProgressBarMgr::boundingRect() const
{
	if (QGraphicsItem::children().size()==0)
		return QRectF();
	const QRectF& r = childrenBoundingRect();
	return QRectF(0, 0, r.width()-1, r.height()-1);
}*/

void StelProgressBarMgr::addProgressBar(const StelProgressController* p)
{
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	QProgressBar* pb = new QProgressBar();
	pb->setFixedHeight(25);
	pb->setFixedWidth(200);
	pb->setTextVisible(true);
	pb->setValue(p->getValue());
	pb->setMinimum(p->getMin());
	pb->setMaximum(p->getMax());
	pb->setFormat(p->getFormat());
	if (gui!=Q_NULLPTR)
		pb->setStyleSheet(gui->getStelStyle().qtStyleSheet);
	QGraphicsProxyWidget* pbProxy = new QGraphicsProxyWidget();
	pbProxy->setWidget(pb);
	pbProxy->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
	pbProxy->setZValue(150);	
	static_cast<QGraphicsLinearLayout*>(layout())->addItem(pbProxy);
	allBars.insert(p, pb);
	pb->setVisible(true);
	
	connect(p, SIGNAL(changed()), this, SLOT(oneBarChanged()));
}

void StelProgressBarMgr::removeProgressBar(const StelProgressController *p)
{
	QProgressBar* pb = allBars[p];
	pb->deleteLater();
	allBars.remove(p);
}

void StelProgressBarMgr::oneBarChanged()
{
	const StelProgressController *p = static_cast<StelProgressController*>(QObject::sender());
	QProgressBar* pb = allBars[p];
	pb->setValue(p->getValue());
	pb->setMinimum(p->getMin());
	pb->setMaximum(p->getMax());
	pb->setFormat(p->getFormat());
}

CornerButtons::CornerButtons(QGraphicsItem* parent) :
	QGraphicsItem(parent),
	lastOpacity(10)
{
}

void CornerButtons::paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*)
{
	// Do nothing. Just paint the child widgets
}

QRectF CornerButtons::boundingRect() const
{
	if (QGraphicsItem::childItems().size()==0)
		return QRectF();
	const QRectF& r = childrenBoundingRect();
	return QRectF(0, 0, r.width()-1, r.height()-1);
}

void CornerButtons::setOpacity(double opacity)
{
	if (opacity<=0. && lastOpacity<=0.)
		return;
	lastOpacity = opacity;
	if (QGraphicsItem::childItems().size()==0)
		return;
	for (auto* child : QGraphicsItem::childItems())
	{
		StelButton* sb = qgraphicsitem_cast<StelButton*>(child);
		Q_ASSERT(sb!=Q_NULLPTR);
		sb->setOpacity(opacity);
	}
}
