// FeaturePanel.h
//
// Generic GenICam property grid: lists every available feature from the
// camera's node map (filtered by visibility) with an inline editor and unit.
#ifndef FEATUREPANEL_H
#define FEATUREPANEL_H

#include <QWidget>

class Camera;
class QTableWidget;
class QComboBox;

class FeaturePanel : public QWidget
{
    Q_OBJECT
public:
    explicit FeaturePanel(Camera *camera, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    Camera *m_camera;
    QTableWidget *m_table;
    QComboBox *m_visibility;
};

#endif // FEATUREPANEL_H
