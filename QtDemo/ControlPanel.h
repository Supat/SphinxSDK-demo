// ControlPanel.h
//
// Targeted controls for the most common camera settings (exposure, gain, frame
// rate, auto modes, white balance). Only features the camera actually exposes
// are shown. Built on the generic feature editors.
#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>

class Camera;
class QFormLayout;

class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanel(Camera *camera, QWidget *parent = nullptr);

public slots:
    void refresh();   // rebuild rows from the connected camera

private:
    Camera *m_camera;
    QFormLayout *m_form;
};

#endif // CONTROLPANEL_H
