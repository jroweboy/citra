// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureLayout;
}

class ConfigureLayout : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigureLayout(QWidget *parent = 0);
    ~ConfigureLayout();

    void applyConfiguration();

private:
    void setConfiguration();

private:
    Ui::ConfigureLayout *ui;
};
