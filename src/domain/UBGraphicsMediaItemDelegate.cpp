/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtGui>
#include <QtSvg>

#include "UBGraphicsMediaItem.h"
#include "UBGraphicsMediaItemDelegate.h"

#include "UBGraphicsScene.h"

#include "core/UBSettings.h"
#include "core/UBApplication.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"

#include "domain/UBGraphicsMediaItem.h"

#include "core/memcheck.h"

UBGraphicsMediaItemDelegate::UBGraphicsMediaItemDelegate(UBGraphicsMediaItem* pDelegated, Phonon::MediaObject* pMedia, QObject * parent)
    : UBGraphicsItemDelegate(pDelegated, parent, true, false)
    , mMedia(pMedia)
    , mToolBarShowTimer(NULL)
    , m_iToolBarShowingInterval(5000)
{
    QPalette palette;
    palette.setBrush ( QPalette::Light, Qt::darkGray );

    mMedia->setTickInterval(50);
    connect(mMedia, SIGNAL(stateChanged (Phonon::State, Phonon::State)), this, SLOT(mediaStateChanged (Phonon::State, Phonon::State)));
    connect(mMedia, SIGNAL(finished()), this, SLOT(updatePlayPauseState()));
    connect(mMedia, SIGNAL(tick(qint64)), this, SLOT(updateTicker(qint64)));
    connect(mMedia, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));

    if (delegated()->hasLinkedImage())
    {
        mToolBarShowTimer = new QTimer();
        connect(mToolBarShowTimer, SIGNAL(timeout()), this, SLOT(hideToolBar()));
        mToolBarShowTimer->setInterval(m_iToolBarShowingInterval);
    }
}

bool UBGraphicsMediaItemDelegate::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    mToolBarItem->show();

    if (mToolBarShowTimer)
        mToolBarShowTimer->start();

    return UBGraphicsItemDelegate::mousePressEvent(event);
}

void UBGraphicsMediaItemDelegate::hideToolBar()
{
    mToolBarItem->hide();
}

void UBGraphicsMediaItemDelegate::buildButtons()
{
    mPlayPauseButton = new DelegateButton(":/images/play.svg", mDelegated, mToolBarItem, Qt::TitleBarArea);
    connect(mPlayPauseButton, SIGNAL(clicked(bool)), this, SLOT(togglePlayPause()));
    connect(mPlayPauseButton, SIGNAL(clicked(bool)), mToolBarShowTimer, SLOT(start()));
    

    mStopButton = new DelegateButton(":/images/stop.svg", mDelegated, mToolBarItem, Qt::TitleBarArea);
    connect(mStopButton, SIGNAL(clicked(bool)), mMedia, SLOT(stop()));
    connect(mStopButton, SIGNAL(clicked(bool)), mToolBarShowTimer, SLOT(start()));

    mMediaControl = new DelegateMediaControl(delegated(), mToolBarItem);
    mMediaControl->setFlag(QGraphicsItem::ItemIsSelectable, true);
    UBGraphicsItem::assignZValue(mMediaControl, delegated()->zValue());
    connect(mMediaControl, SIGNAL(used()), mToolBarShowTimer, SLOT(start()));
    
    if (delegated()->isMuted())
        mMuteButton = new DelegateButton(":/images/soundOff.svg", mDelegated, mToolBarItem, Qt::TitleBarArea);
    else
        mMuteButton = new DelegateButton(":/images/soundOn.svg", mDelegated, mToolBarItem, Qt::TitleBarArea);

    connect(mMuteButton, SIGNAL(clicked(bool)), delegated(), SLOT(toggleMute())); 
    connect(mMuteButton, SIGNAL(clicked(bool)), this, SLOT(toggleMute())); // for changing button image
    connect(mMuteButton, SIGNAL(clicked(bool)), mToolBarShowTimer, SLOT(start()));

    mButtons << mPlayPauseButton << mStopButton << mMuteButton;

    mToolBarItem->setItemsOnToolBar(QList<QGraphicsItem*>() << mPlayPauseButton << mStopButton << mMediaControl << mMuteButton);
    mToolBarItem->setVisibleOnBoard(true);
    mToolBarItem->setShifting(false);

    UBGraphicsMediaItem *audioItem = dynamic_cast<UBGraphicsMediaItem*>(mDelegated);
    if (audioItem)
    {
        if (audioItem->getMediaType() == UBGraphicsMediaItem::mediaType_Audio)
        {
            positionHandles();
        }
    }
}

UBGraphicsMediaItemDelegate::~UBGraphicsMediaItemDelegate()
{
    if (mToolBarShowTimer)
        delete mToolBarShowTimer;
}

void UBGraphicsMediaItemDelegate::positionHandles()
{
    UBGraphicsItemDelegate::positionHandles();

    qreal AntiScaleRatio = 1 / (UBApplication::boardController->systemScaleFactor() * UBApplication::boardController->currentZoom());           

    UBGraphicsMediaItem *mediaItem = dynamic_cast<UBGraphicsMediaItem*>(mDelegated);
    if (mediaItem)
    {
        if (mediaItem->getMediaType() != UBGraphicsMediaItem::mediaType_Audio)
        {
            mToolBarItem->setPos(0, delegated()->boundingRect().height()-mToolBarItem->rect().height()*AntiScaleRatio);
            mToolBarItem->setScale(AntiScaleRatio);
            QRectF toolBarRect = mToolBarItem->rect();
            toolBarRect.setWidth(delegated()->boundingRect().width()/AntiScaleRatio);
            mToolBarItem->setRect(toolBarRect);           
        }
        else
        {
            mToolBarItem->setPos(0, 0);
            mToolBarItem->show();
        }
    }

    int mediaItemWidth = mToolBarItem->boundingRect().width();
    foreach (DelegateButton* button, mButtons)
    {
        if (button->getSection() == Qt::TitleBarArea)
            mediaItemWidth -= button->boundingRect().width();
    }

    QRectF mediaItemRect = mMediaControl->rect();
    mediaItemRect.setWidth(mediaItemWidth);
    mediaItemRect.setHeight(mToolBarItem->boundingRect().height());
    mMediaControl->setRect(mediaItemRect);

    mToolBarItem->positionHandles();
    mMediaControl->positionHandles(); 

    if (mediaItem)
    {
        if (mediaItem->getMediaType() == UBGraphicsMediaItem::mediaType_Audio)
        {
            mToolBarItem->show();
        }
    }
}

void UBGraphicsMediaItemDelegate::remove(bool canUndo)
{
    if (delegated() && delegated()->mediaObject())
        delegated()->mediaObject()->stop();

    QGraphicsScene* scene = mDelegated->scene();

    scene->removeItem(mMediaControl);

    UBGraphicsItemDelegate::remove(canUndo);
}


void UBGraphicsMediaItemDelegate::toggleMute()
{
    if (delegated()->isMuted())
        mMuteButton->setFileName(":/images/soundOff.svg");
    else
        mMuteButton->setFileName(":/images/soundOn.svg");
}


UBGraphicsMediaItem* UBGraphicsMediaItemDelegate::delegated()
{
    return dynamic_cast<UBGraphicsMediaItem*>(mDelegated);
}


void UBGraphicsMediaItemDelegate::togglePlayPause()
{
    if (delegated() && delegated()->mediaObject()) {

        Phonon::MediaObject* media = delegated()->mediaObject();
        if (media->state() == Phonon::StoppedState) {
            media->play();
        } else if (media->state() == Phonon::PlayingState) {
            if (media->remainingTime() <= 0) {
                media->stop();
                media->play();
            } else {
                media->pause();
                if(delegated()->scene())
                        delegated()->scene()->setModified(true);
            }
        } else if (media->state() == Phonon::PausedState) {
            if (media->remainingTime() <= 0) {
                media->stop();
            }
            media->play();
        } else  if ( media->state() == Phonon::LoadingState ) {
            delegated()->mediaObject()->setCurrentSource(delegated()->mediaFileUrl());
            media->play();
        } else if (media->state() == Phonon::ErrorState){
            qDebug() << "Error appeared." << media->errorString();
        }
    }
}

void UBGraphicsMediaItemDelegate::mediaStateChanged ( Phonon::State newstate, Phonon::State oldstate )
{
    Q_UNUSED(newstate);
    Q_UNUSED(oldstate);
    updatePlayPauseState();
}


void UBGraphicsMediaItemDelegate::updatePlayPauseState()
{
    Phonon::MediaObject* media = delegated()->mediaObject();

    if (media->state() == Phonon::PlayingState)
        mPlayPauseButton->setFileName(":/images/pause.svg");
    else
        mPlayPauseButton->setFileName(":/images/play.svg");
}


void UBGraphicsMediaItemDelegate::updateTicker(qint64 time)
{
    Phonon::MediaObject* media = delegated()->mediaObject();
    mMediaControl->totalTimeChanged(media->totalTime());
    mMediaControl->updateTicker(time);
}


void UBGraphicsMediaItemDelegate::totalTimeChanged(qint64 newTotalTime)
{
    mMediaControl->totalTimeChanged(newTotalTime);
}