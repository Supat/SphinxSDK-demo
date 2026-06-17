// ControlPanel.cpp
#include "ControlPanel.h"
#include "FeatureEditor.h"
#include "Camera.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

namespace {
// Curated list of common settings: GenICam node name + friendly label.
struct Target { const char *name; const char *label; };
const Target kTargets[] = {
    {"ExposureAuto",              "Exposure Auto"},
    {"ExposureTime",             "Exposure"},
    {"GainAuto",                 "Gain Auto"},
    {"Gain",                     "Gain"},
    {"BlackLevel",               "Black Level"},
    {"AcquisitionFrameRateEnable","Frame Rate Enable"},
    {"AcquisitionFrameRate",     "Frame Rate"},
    {"BalanceWhiteAuto",         "White Balance Auto"},
    {"TriggerMode",              "Trigger Mode"},
    {"TriggerSource",            "Trigger Source"},
};
} // namespace

ControlPanel::ControlPanel(Camera *camera, QWidget *parent)
    : QWidget(parent), m_camera(camera)
{
    auto *outer = new QVBoxLayout(this);

    auto *refreshBtn = new QPushButton("Re-read values");
    connect(refreshBtn, &QPushButton::clicked, this, &ControlPanel::refresh);
    outer->addWidget(refreshBtn);

    auto *formHost = new QWidget;
    m_form = new QFormLayout(formHost);
    outer->addWidget(formHost);
    outer->addStretch(1);
}

void ControlPanel::refresh()
{
    // Clear existing rows.
    while (m_form->rowCount() > 0)
        m_form->removeRow(0);

    if (!m_camera->isOpen())
    {
        m_form->addRow(new QLabel("Connect a camera to see controls."));
        return;
    }

    int shown = 0;
    for (const Target &t : kTargets)
    {
        FeatureInfo fi = m_camera->describeFeature(t.name);
        if (!fi.available || fi.type == FeatureType::Unknown)
            continue;
        QWidget *editor = makeFeatureEditor(m_camera, fi, this);
        m_form->addRow(QString("%1:").arg(t.label), editor);
        ++shown;
    }
    if (shown == 0)
        m_form->addRow(new QLabel("No common controls exposed by this camera."));
}
