// FeaturePanel.cpp
#include "FeaturePanel.h"
#include "FeatureEditor.h"
#include "Camera.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

FeaturePanel::FeaturePanel(Camera *camera, QWidget *parent)
    : QWidget(parent), m_camera(camera)
{
    auto *outer = new QVBoxLayout(this);

    auto *header = new QHBoxLayout;
    header->addWidget(new QLabel("Visibility:"));
    m_visibility = new QComboBox;
    m_visibility->addItem("Beginner", 0);
    m_visibility->addItem("Expert", 2);
    m_visibility->addItem("Guru", 3);
    m_visibility->addItem("All", 99);
    header->addWidget(m_visibility);
    auto *refreshBtn = new QPushButton("Refresh");
    header->addWidget(refreshBtn);
    header->addStretch(1);
    outer->addLayout(header);

    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({"Feature", "Value", "Unit"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    outer->addWidget(m_table);

    connect(refreshBtn, &QPushButton::clicked, this, &FeaturePanel::refresh);
    connect(m_visibility, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FeaturePanel::refresh);
}

void FeaturePanel::refresh()
{
    m_table->setRowCount(0);
    if (!m_camera->isOpen())
        return;

    const int maxLevel = m_visibility->currentData().toInt();
    const QVector<FeatureInfo> features = m_camera->featureList(maxLevel);

    m_table->setRowCount(features.size());
    int row = 0;
    for (const FeatureInfo &fi : features)
    {
        auto *nameItem = new QTableWidgetItem(fi.displayName);
        nameItem->setToolTip(fi.tooltip.isEmpty() ? fi.name : fi.tooltip);
        m_table->setItem(row, 0, nameItem);

        m_table->setCellWidget(row, 1, makeFeatureEditor(m_camera, fi, m_table));
        m_table->setItem(row, 2, new QTableWidgetItem(fi.unit));
        ++row;
    }
}
