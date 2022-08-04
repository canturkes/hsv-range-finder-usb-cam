#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QString>
#include <QImage>
#include <QPixmap>

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

using namespace cv;

namespace
{
    std::atomic<int> Hlow = 0, Hhigh = 255,
                    Slow = 0, Shigh = 255,
                    Vlow = 0, Vhigh = 255;

    std::atomic<bool> WORKING_PERMIT = false;
    std::atomic<bool> THREAD_WORKING = false;
    std::atomic<bool> BINARY_OUTPUT = false;

    std::atomic<int> CAM_NO = 1;

    int fps = 30;
}


void CaptureThread(Ui::MainWindow* ui)
{
    THREAD_WORKING.store(true, std::memory_order_release);

    cv::VideoCapture cam(CAM_NO);

    if (cam.isOpened())
    {
        ui->statusbar->showMessage("Capturing...");

        cv::Mat frame;
        cv::Mat frame_hsv;
        cv::Mat mask;
        cv::Mat mask_rgb;
        cv::Mat colored_roi;

        QImage img_r, img_p;
        QPixmap pixel_r, pixel_p;

        int sleep_ms = 1000 / fps;

        while (WORKING_PERMIT.load(std::memory_order_acquire))
        {
            cam >> frame;

            if (frame.empty())
            {
                ui->statusbar->showMessage("Camera disconnected (empty frame error)");
                break;
            }

            cv::cvtColor(frame, frame_hsv, COLOR_BGR2HSV);
            cv::inRange(frame_hsv,
                        Scalar(Hlow, Slow, Vlow),
                        Scalar(Hhigh, Shigh, Vhigh),
                        mask);

            if (BINARY_OUTPUT.load())
            {
                cv::cvtColor(mask, mask_rgb, COLOR_GRAY2BGR);
            }
            else
            {
                colored_roi = frame.clone();
                colored_roi.setTo(Scalar(0,0,0), mask == 0);
            }

            img_r = QImage( (uchar*) frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
            pixel_r = QPixmap::fromImage(img_r);
            ui->label_raw_frame->setPixmap(pixel_r);

            if (BINARY_OUTPUT.load())
                img_p = QImage( (uchar*) mask_rgb.data, mask_rgb.cols, mask_rgb.rows, mask_rgb.step, QImage::Format_BGR888);
            else img_p = QImage( (uchar*) colored_roi.data, colored_roi.cols, colored_roi.rows, colored_roi.step, QImage::Format_BGR888);
            pixel_p = QPixmap::fromImage(img_p);
            ui->label_proc_frame->setPixmap(pixel_p);

            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }

        // clear on exit
        ui->label_proc_frame->clear();
        ui->label_raw_frame->clear();
    }
    else
    {
        ui->statusbar->showMessage("Cannot open camera with ID: " + QString::number(CAM_NO.load()));
    }

    THREAD_WORKING.store(false, std::memory_order_release);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Stop Capture button is disabled at the start.
    ui->buttonStopCapture->setDisabled(true);

    // Show welcome message.
    ui->statusbar->showMessage("Welcome to HSV Range Finder for USB Camera!");
}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_spinBoxCamNo_valueChanged(int arg1)
{
    if (!THREAD_WORKING.load(std::memory_order_acquire))
    {
        CAM_NO.store(arg1);
    }
    else
    {
        ui->spinBoxCamNo->setValue(CAM_NO.load());
    }
}


void MainWindow::on_buttonStartCapture_clicked()
{
    // Don't start the process if it's already running.
    if (!THREAD_WORKING.load(std::memory_order_acquire))
    {
        // Disable cam no. input
        ui->spinBoxCamNo->setDisabled(true);

        // Disable start capture button
        ui->buttonStartCapture->setDisabled(true);

        // Enable start capture button
        ui->buttonStopCapture->setEnabled(true);

        // Activate working permit
        WORKING_PERMIT.store(true, std::memory_order_release);

        // Start capture thread
        std::thread capture_thread(CaptureThread, std::ref(ui));

        // Then detach it
        capture_thread.detach();

        // Wait for thread activation
        while (!THREAD_WORKING.load(std::memory_order_acquire))
            std::this_thread::yield();
    }
    else return;
}


void MainWindow::on_buttonStopCapture_clicked()
{
    // Deactivate working permit
    WORKING_PERMIT.store(false, std::memory_order_release);

    // Wait for exit
    if (THREAD_WORKING.load(std::memory_order_acquire))
    {
        while(THREAD_WORKING.load(std::memory_order_acquire))
            std::this_thread::yield();
    }

    // Disable stop capture button
    ui->buttonStopCapture->setDisabled(true);

    // Enable start capture button
    ui->buttonStartCapture->setEnabled(true);

    // Enable cam no. input
    ui->spinBoxCamNo->setEnabled(true);

    // Show message
    ui->statusbar->showMessage("No capture");
}


void MainWindow::on_checkBoxBinaryOut_clicked(bool checked)
{
    BINARY_OUTPUT.store(checked);
}


// ****************** SLIDERS & INPUTS *********/

// H (hue)

void MainWindow::on_spinBox_H_low_valueChanged(int arg1)
{
    if (arg1 <= Hhigh.load())
    {
        Hlow.store(arg1);
        ui->horizontalSlider_H_low->setValue(arg1);
    }
    else
    {
        Hlow.store(Hhigh.load());
        ui->horizontalSlider_H_low->setValue(Hhigh.load());
        ui->spinBox_H_low->setValue(Hhigh.load());
    }
}


void MainWindow::on_horizontalSlider_H_low_sliderMoved(int position)
{
    if (position <= Hhigh.load())
    {
        Hlow.store(position);
        ui->spinBox_H_low->setValue(position);
    }
    else
    {
        Hlow.store(Hhigh.load());
        ui->spinBox_H_low->setValue(Hhigh.load());
        ui->horizontalSlider_H_low->setValue(Hhigh.load());
    }
}


void MainWindow::on_spinBox_H_high_valueChanged(int arg1)
{
    if (arg1 >= Hlow.load())
    {
        Hhigh.store(arg1);
        ui->horizontalSlider_H_high->setValue(arg1);
    }
    else
    {
        Hhigh.store(Hlow.load());
        ui->horizontalSlider_H_high->setValue(Hlow.load());
        ui->spinBox_H_high->setValue(Hlow.load());
    }
}


void MainWindow::on_horizontalSlider_H_high_sliderMoved(int position)
{
    if (position >= Hlow.load())
    {
        Hhigh.store(position);
        ui->spinBox_H_high->setValue(position);
    }
    else
    {
        Hhigh.store(Hlow.load());
        ui->spinBox_H_high->setValue(Hlow.load());
        ui->horizontalSlider_H_high->setValue(Hlow.load());
    }
}


// S (saturation)

void MainWindow::on_spinBox_S_low_valueChanged(int arg1)
{
    if (arg1 <= Shigh.load())
    {
        Slow.store(arg1);
        ui->horizontalSlider_S_low->setValue(arg1);
    }
    else
    {
        Slow.store(Shigh.load());
        ui->horizontalSlider_S_low->setValue(Shigh.load());
        ui->spinBox_S_low->setValue(Shigh.load());
    }
}


void MainWindow::on_horizontalSlider_S_low_sliderMoved(int position)
{
    if (position <= Shigh.load())
    {
        Slow.store(position);
        ui->spinBox_S_low->setValue(position);
    }
    else
    {
        Slow.store(Shigh.load());
        ui->spinBox_S_low->setValue(Shigh.load());
        ui->horizontalSlider_S_low->setValue(Shigh.load());
    }
}


void MainWindow::on_spinBox_S_high_valueChanged(int arg1)
{
    if (arg1 >= Slow.load())
    {
        Shigh.store(arg1);
        ui->horizontalSlider_S_high->setValue(arg1);
    }
    else
    {
        Shigh.store(Slow.load());
        ui->horizontalSlider_S_high->setValue(Slow.load());
        ui->spinBox_S_high->setValue(Slow.load());
    }
}


void MainWindow::on_horizontalSlider_S_high_sliderMoved(int position)
{
    if (position >= Slow.load())
    {
        Shigh.store(position);
        ui->spinBox_S_high->setValue(position);
    }
    else
    {
        Shigh.store(Slow.load());
        ui->spinBox_S_high->setValue(Slow.load());
        ui->horizontalSlider_S_high->setValue(Slow.load());
    }
}


// V

void MainWindow::on_spinBox_V_low_valueChanged(int arg1)
{
    if (arg1 <= Vhigh.load())
    {
        Vlow.store(arg1);
        ui->horizontalSlider_V_low->setValue(arg1);
    }
    else
    {
        Vlow.store(Vhigh.load());
        ui->horizontalSlider_V_low->setValue(Vhigh.load());
        ui->spinBox_V_low->setValue(Vhigh.load());
    }
}


void MainWindow::on_horizontalSlider_V_low_sliderMoved(int position)
{
    if (position <= Vhigh.load())
    {
        Vlow.store(position);
        ui->spinBox_V_low->setValue(position);
    }
    else
    {
        Vlow.store(Vhigh.load());
        ui->spinBox_V_low->setValue(Vhigh.load());
        ui->horizontalSlider_V_low->setValue(Vhigh.load());
    }
}


void MainWindow::on_spinBox_V_high_valueChanged(int arg1)
{
    if (arg1 >= Vlow.load())
    {
        Vhigh.store(arg1);
        ui->horizontalSlider_V_high->setValue(arg1);
    }
    else
    {
        Vhigh.store(Vlow.load());
        ui->horizontalSlider_V_high->setValue(Vlow.load());
        ui->spinBox_V_high->setValue(Vlow.load());
    }
}


void MainWindow::on_horizontalSlider_V_high_sliderMoved(int position)
{
    if (position >= Vlow.load())
    {
        Vhigh.store(position);
        ui->spinBox_V_high->setValue(position);
    }
    else
    {
        Vhigh.store(Vlow.load());
        ui->spinBox_V_high->setValue(Vlow.load());
        ui->horizontalSlider_V_high->setValue(Vlow.load());
    }
}

