/******************************************************************************
    VideoWall:  this file is part of QtAV examples
    Copyright (C) 2012-2013 Wang Bin <wbsecg1@gmail.com>

*   This file is part of QtAV

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "VideoWall.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QtCore/QUrl>
#include <QtAV/AudioOutput.h>

using namespace QtAV;
const int kSyncInterval = 2000;

VideoWall::VideoWall(QObject *parent) :
    QObject(parent),r(3),c(3),view(0),menu(0)
{
    clock = new AVClock(this);
    clock->setClockType(AVClock::ExternalClock);
    //view = new QWidget;
    if (view) {
        qDebug("WA_OpaquePaintEvent=%d", view->testAttribute(Qt::WA_OpaquePaintEvent));
        view->resize(qApp->desktop()->size());
        view->move(QPoint(0, 0));
        view->show();
    }
}

VideoWall::~VideoWall()
{
    if (menu) {
        delete menu;
        menu = 0;
    }
    if (!players.isEmpty()) {
        foreach (AVPlayer *player, players) {
            player->stop();
            WidgetRenderer* renderer = static_cast<WidgetRenderer*>(player->renderer());
            if (renderer) {
                renderer->QWidget::close(); //TODO: rename
                if (!renderer->testAttribute(Qt::WA_DeleteOnClose))
                    delete renderer;
                delete player;
            }
        }
        players.clear();
    }
    delete view;
}

void VideoWall::setRows(int n)
{
    r = n;
}

void VideoWall::setCols(int n)
{
    c = n;
}

int VideoWall::rows() const
{
    return r;
}

int VideoWall::cols() const
{
    return c;
}

void VideoWall::show()
{
    if (!players.isEmpty()) {
        foreach (AVPlayer *player, players) {
            player->stop();
            WidgetRenderer* renderer = static_cast<WidgetRenderer*>(player->renderer());
            if (renderer) {
                renderer->QWidget::close(); //TODO: rename
                if (!renderer->testAttribute(Qt::WA_DeleteOnClose))
                    delete renderer;
                delete player;
            }
        }
        players.clear();
    }
    qDebug("show wall: %d x %d", r, c);

    int w = view ? view->frameGeometry().width()/c : qApp->desktop()->width()/c;
    int h = view ? view->frameGeometry().height()/r : qApp->desktop()->height()/r;
    for (int i = 0; i < r; ++i) {
        for (int j = 0; j < c; ++j) {
            WidgetRenderer* renderer = new WidgetRenderer(view);
            renderer->setWindowFlags(Qt::FramelessWindowHint);
            renderer->resize(w, h);
            renderer->move(j*w, i*h);
            renderer->show();
            AVPlayer *player = new AVPlayer;
            player->setRenderer(renderer);
            player->setPlayerEventFilter(this);
            player->masterClock()->setClockAuto(false);
            player->masterClock()->setClockType(AVClock::ExternalClock);
            players.append(player);
        }
    }
}

void VideoWall::play(const QString &file)
{
    if (players.isEmpty())
        return;
    clock->reset();
    clock->start();
    foreach (AVPlayer *player, players) {
        player->play(file);
    }
    timer_id = startTimer(kSyncInterval);
}

void VideoWall::stop()
{
    clock->reset();
    killTimer(timer_id);
    foreach (AVPlayer* player, players) {
        player->stop(); //check playing?
    }
}

void VideoWall::openLocalFile()
{
    QString file = QFileDialog::getOpenFileName(0, tr("Open a video"));
    if (file.isEmpty())
        return;
    stop();
    foreach (AVPlayer* player, players) {
        player->load(file);
    }
    clock->reset();
    clock->start();
    timer_id = startTimer(kSyncInterval);
    foreach (AVPlayer* player, players) {
        player->play();
    }
}

void VideoWall::openUrl()
{
    QString url = QInputDialog::getText(0, tr("Open an url"), tr("Url"));
    if (url.isEmpty())
        return;
    stop();
    foreach (AVPlayer* player, players) {
        player->load(url);
    }
    clock->reset();
    clock->start();
    timer_id = startTimer(kSyncInterval);
    foreach (AVPlayer* player, players) {
        player->play();
    }
}

void VideoWall::about()
{
    QMessageBox::about(0, tr("About QtAV"), tr("This is a demo for playing and synchronising multiple players")
                       + "\n\n"
                       + aboutQtAV());
}

void VideoWall::help()
{
    QMessageBox::about(0, tr("Help"),
            "Command line: %1 [-r rows=3] [-c cols=3] path/of/video\n"
       + tr("Drag and drop a file to player\n")
       + tr("Shortcut:\n"
            "Space: pause/continue\n"
            "N: show next frame. Continue the playing by pressing 'Space'\n"
            "O: open a file\n"
            "P: replay\n"
            "S: stop\n"
            "M: mute on/off\n"
            "Up/Down: volume +/-\n"
            "->/<-: seek forward/backward\n"));
}

bool VideoWall::eventFilter(QObject *watched, QEvent *event)
{
    //qDebug("EventFilter::eventFilter to %p", watched);
    Q_UNUSED(watched);
    if (players.isEmpty())
        return false;
    QEvent::Type type = event->type();
    switch (type) {
    case QEvent::KeyPress: {
        //qDebug("Event target = %p %p", watched, player->renderer);
        //avoid receive an event multiple times
        int key = static_cast<QKeyEvent*>(event)->key();
        switch (key) {
        case Qt::Key_N: //check playing?
            foreach (AVPlayer* player, players) {
                player->playNextFrame();
            }
            break;
        case Qt::Key_P:
            clock->reset();
            clock->start();
            foreach (AVPlayer* player, players) {
                player->play();
            }
            timer_id = startTimer(kSyncInterval);
            break;
        case Qt::Key_S:
            stop();
            break;
        case Qt::Key_Space: //check playing?
            clock->pause(!clock->isActive());
            foreach (AVPlayer* player, players) {
                player->pause(!player->isPaused());
            }
            break;
        case Qt::Key_Up:
            foreach (AVPlayer* player, players) {
                if (player->audio()) {
                    qreal v = player->audio()->volume();
                    if (v > 0.5)
                        v += 0.1;
                    else if (v > 0.1)
                        v += 0.05;
                    else
                        v += 0.025;
                    player->audio()->setVolume(v);
                }
            }
            break;
        case Qt::Key_Down:
            foreach (AVPlayer* player, players) {
                if (player->audio()) {
                    qreal v = player->audio()->volume();
                    if (v > 0.5)
                        v -= 0.1;
                    else if (v > 0.1)
                        v -= 0.05;
                    else
                        v -= 0.025;
                    player->audio()->setVolume(v);
                }
            }


            break;
        case Qt::Key_O:
            openLocalFile();
            break;
        case Qt::Key_Left:
            qDebug("<-");
            clock->updateExternalClock(clock->value()*1000.0 - 2000.0);
            /*foreach (AVPlayer* player, players) {
                player->seekBackward();
            }*/
            break;
        case Qt::Key_Right:
            qDebug("->");
            clock->updateExternalClock(clock->value()*1000.0 + 2000.0);
            /*foreach (AVPlayer* player, players) {
                player->seekForward();
            }*/
            break;
        case Qt::Key_M:
            foreach (AVPlayer* player, players) {
                if (player->audio()) {
                    player->audio()->setMute(!player->audio()->isMute());
                }
            }
            break;
        default:
            return false;
        }
        break;
    }
    case QEvent::ContextMenu: {
        QContextMenuEvent *e = static_cast<QContextMenuEvent*>(event);
        if (!menu) {
            menu = new QMenu();
            menu->addAction(tr("Open"), this, SLOT(openLocalFile()));
            menu->addAction(tr("Open Url"), this, SLOT(openUrl()));
            menu->addSeparator();
            menu->addAction(tr("About"), this, SLOT(about()));
            menu->addAction(tr("Help"), this, SLOT(help()));
            menu->addSeparator();
            menu->addAction(tr("About Qt"), qApp, SLOT(aboutQt()));
        }
        menu->popup(e->globalPos());
        menu->exec();
    }
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        QDropEvent *e = static_cast<QDropEvent*>(event);
        e->acceptProposedAction();
    }
        break;
    case QEvent::Drop: {
        QDropEvent *e = static_cast<QDropEvent*>(event);
        QString path = e->mimeData()->urls().first().toLocalFile();
        stop();
        play(path);
        e->acceptProposedAction();
    }
        break;
    default:
        return false;
    }
    return true; //false: for text input
}

void VideoWall::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != timer_id) {
        qDebug("Not clock id");
        return;
    }
    if (!clock->isActive()) {
        qDebug("clock not running");
        return;
    }
    foreach (AVPlayer *player, players) {
        player->masterClock()->updateExternalClock(*clock);
    }
}
