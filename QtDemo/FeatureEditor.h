// FeatureEditor.h
//
// Factory that turns a FeatureInfo into a live editor widget bound to the
// camera, plus a slider+spinbox composite for numeric features. Shared by the
// targeted ControlPanel and the generic FeaturePanel.
#ifndef FEATUREEDITOR_H
#define FEATUREEDITOR_H

#include <QWidget>
#include "Camera.h"

class QSlider;
class QDoubleSpinBox;

// Slider + spin box kept in sync, writing Integer/Float features on change.
class NumericFeatureWidget : public QWidget
{
    Q_OBJECT
public:
    NumericFeatureWidget(Camera *camera, const FeatureInfo &info, QWidget *parent = nullptr);
    void reload();   // re-read current value from the camera

private slots:
    void onSpinChanged(double v);
    void onSliderMoved(int tick);

private:
    void setValueQuiet(double v);

    Camera *m_camera;
    FeatureInfo m_info;
    QSlider *m_slider = nullptr;
    QDoubleSpinBox *m_spin = nullptr;
    bool m_guard = false;
};

// Build the appropriate editor for a feature (numeric/enum/bool/command/string),
// initialised to its current value and wired to write back. Never returns null.
QWidget *makeFeatureEditor(Camera *camera, const FeatureInfo &info, QWidget *parent = nullptr);

#endif // FEATUREEDITOR_H
