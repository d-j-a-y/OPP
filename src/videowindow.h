/**********************************************************************************
 * This file is part of Open Projection Program (OPP).
 *
 * Copyright (C) 2013 Catalogue Ouvert du Cinéma <dev@cinemaouvert.fr>
 *
 * Authors: Florian Mhun <florian.mhun@gmail.com>
 *          Thibaud Lamarche <lamarchethibaud@hotmail.com>
 *
 * Open Projection Program is an initiative of Catalogue Ouvert du Cinéma.
 * The software was developed by four students of University of Poitiers
 * as school project.
 *
 * Open Projection Program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Open Projection Program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Projection Program. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************************/

#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H

#include <QMainWindow>
#include <QShortcut>

#include "videowidget.h"

class VideoWindow : public QMainWindow
{
    Q_OBJECT
public:

    /**
     * @brief Contains display mode values
     *
     * @author Florian Mhun <florian.mhun@gmail.com>
     */
    enum DisplayMode {
        /**
         * Video onto a classical window
         */
        WINDOW = 0,

        /**
         * Fullscreen video into the 2nd display
         */
        PROJECTION = 1
    };

    VideoWindow(QWidget *parent, const DisplayMode &mode);

    VideoWindow(QWidget *parent = 0, int width = 640, int height = 480);

    ~VideoWindow();

    /**
     * @brief Returns the video widget
     * @return The video widget
     *
     * @author Florian Mhun <florian.mhun@gmail.com>
     */
    inline VideoWidget* videoWidget() const { return _videoWidget; }

    /**
     * @brief Change the display mode
     * @param mode The display mode to apply
     *
     * @author Florian Mhun <florian.mhun@gmail.com>
     */
    void setDisplayMode(const DisplayMode &mode);

    /**
     * @brief Move the video window
     * @param display
     *
     * @author Florian Mhun <florian.mhun@gmail.com>
     */
    void moveToDisplay(const int &display);

    /**
     * @brief Shut down the video when closing the window
     * @param event
     *
     * @author Thibaud Lamarche <lamarchethibaud@hotmail.fr>
     */
    void closeEvent (QCloseEvent *event);

    /**
     * @brief Connect the PlayerControlWidget shortcuts to the VideoWindow
     *
     * @author Jerome Blanchi <d.j.a.y@free.fr>
     */
    void initShortcuts();

private slots:
    void switchVideoMode();

    void escapeFullscreen();

signals:
    /**
     * @brief Signal launch when we close the window
     *
     * @author Thibaud Lamarche <lamarchethibaud@hotmail.fr>
     */
    void closed();

private:
    /**
     * @brief _videoWidget The video widget
     */
    VideoWidget *_videoWidget;

    /**
     * @brief _mode The display mode
     */
    DisplayMode _mode;

    QShortcut* _f1_shortcut;

    QShortcut* _escape_shortcut;

    QShortcut* _playPause_shortcut;
    QShortcut* _previous_shortcut;
    QShortcut* _next_shortcut;
    QShortcut* _rewind_shortcut;
    QShortcut* _forward_shortcut;

    /**
     * @brief initVideoWindow Shared initialization between constructors.
     */
    void initVideoWindow();
};

#endif // VIDEOWINDOW_H
