/*=========================================================================

Library:   AnatomicAugmentedRealityProjector

Author: Maeliss Jallais

Copyright 2010 Kitware Inc. 28 Corporate Drive,
Clifton Park, NY, 12065, USA.

All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=========================================================================*/

#include "CameraInput.hpp"

#include "FlyCapture2.h"

#include <opencv2/imgproc/imgproc.hpp>

#include <QTime>

#include <iomanip>
#include <iostream>
#include <stdio.h>
//#include <ostream>

using namespace FlyCapture2;


CameraInput::CameraInput() : NbImages( 1 ), Camera()
{
}


CameraInput::~CameraInput()
{
  Error error;
  // Stop capturing images
  error = this->Camera.StopCapture();
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    }

  // Disconnect the camera
  error = this->Camera.Disconnect();
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    }
}


bool CameraInput::Configure()
{
  Error error;
  Property prop;

  /********** Custom Video Mode : Mode 2 - RAW8 pixel type **********/

  const Mode k_fmt7Mode = MODE_2;
  const PixelFormat k_fmt7PixFmt = PIXEL_FORMAT_RAW8;

  // Query for available Format 7 modes
  Format7Info fmt7Info;
  bool supported;
  fmt7Info.mode = k_fmt7Mode;
  error = Camera.GetFormat7Info( &fmt7Info, &supported );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }
  if( ( k_fmt7PixFmt & fmt7Info.pixelFormatBitField ) == 0 )
    {
    // Pixel format not supported!
    std::cout << "Pixel format is not supported" << std::endl;
    return false;
    }

  Format7ImageSettings fmt7ImageSettings;
  fmt7ImageSettings.mode = k_fmt7Mode;
  fmt7ImageSettings.offsetX = 0;
  fmt7ImageSettings.offsetY = 0;
  fmt7ImageSettings.width = fmt7Info.maxWidth;
  fmt7ImageSettings.height = fmt7Info.maxHeight;
  fmt7ImageSettings.pixelFormat = k_fmt7PixFmt;

  bool valid;
  Format7PacketInfo fmt7PacketInfo;

  // Validate the settings to make sure that they are valid
  error = Camera.ValidateFormat7Settings(
    &fmt7ImageSettings,
    &valid,
    &fmt7PacketInfo );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  if( !valid )
    {
    // Settings are not valid
    std::cout << "Format7 settings are not valid" << std::endl;
    return false;
    }

  // Set the settings to the camera
  error = Camera.SetFormat7Configuration(
    &fmt7ImageSettings,
    fmt7PacketInfo.recommendedBytesPerPacket );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  /********** Advanced Camera Settings : Shutter Min=1 Max=2 (=0.009ms) **********/

  prop.type = SHUTTER;
  prop.onOff = true;
  prop.autoManualMode = false;
  prop.absControl = true;
  prop.absValue = 0.009f;
  error = Camera.SetProperty( &prop );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  /********** Trigger Enable, Mode 0 Parameter 0, trigger input source = GPIO0, Trigger Delay Enable = 0 **********/

  TriggerMode mTrigger;
  mTrigger.mode = 0;
  mTrigger.source = 0;
  mTrigger.parameter = 0;
  mTrigger.onOff = true;
  mTrigger.polarity = 0;
  error = Camera.SetTriggerMode( &mTrigger );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }
  this->SetCameraTriggerDelay( 0.0 );

  /********** Camera Settings : Brightness = 0% && Exposure = -2.0 EV **********/

  prop.type = BRIGHTNESS;
  prop.absControl = true;
  prop.absValue = 0;
  error = Camera.SetProperty( &prop );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  prop.type = AUTO_EXPOSURE;
  prop.onOff = true;
  prop.autoManualMode = false;
  prop.absControl = true;
  prop.absValue = -2.0;
  error = Camera.SetProperty( &prop );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  return true;
}


bool CameraInput::Run()
{
  Error error;
  BusManager busMgr;
  //sleep(5);
  cv::waitKey( 2 );
  PGRGuid guid;
  unsigned int numCameras;

  error = busMgr.GetNumOfCameras( &numCameras );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }
  if( numCameras < 1 )
    {
    std::cout << "No camera detected." << std::endl;
    return false;
    }
  else
    {
    std::cout << "Number of cameras detected: " << numCameras << std::endl;
    }

  error = busMgr.GetCameraFromIndex( 0, &guid );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  error = Camera.Connect( &guid );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  this->Configure();
  //this->SetCameraFrameRate(this->FrameRate);

  error = Camera.StartCapture();
  if( error == PGRERROR_ISOCH_BANDWIDTH_EXCEEDED )
    {
    std::cout << "Bandwidth exceeded" << std::endl;
    return false;
    }
  else if( error != PGRERROR_OK )
    {
    std::cout << "Failed to start image capture" << std::endl;
    return false;
    }

  // Retrieve frame rate property
  Property frmRate;
  frmRate.type = FRAME_RATE;
  error = Camera.GetProperty( &frmRate );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }

  std::cout << "Frame rate is " << std::fixed << std::setprecision( 2 ) << frmRate.absValue << " fps" << std::endl;
  return true;
}


bool CameraInput::Stop()
{
  // Stop and disconnect the camera
  BusManager busMgr;

  FlyCapture2::Error error = Camera.StopCapture();
  if( error != FlyCapture2::PGRERROR_OK )
    {
    error.PrintErrorTrace();
    }

  error = Camera.Disconnect();
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return false;
    }
  return true;
}


void CameraInput::IncrementTriggerDelay()
{
  this->delay += .0002;
  if( this->delay > .011 )
    {
    this->delay = 0;
    }
  this->SetCameraTriggerDelay( this->delay );
}


void CameraInput::SetCameraTriggerDelay( double delay )
{
  Error error;

  // Check if the camera supports the TRIGGER_DELAY property
  PropertyInfo propInfo;
  propInfo.type = TRIGGER_DELAY;
  error = this->Camera.GetPropertyInfo( &propInfo );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return;
    }
  if( propInfo.present == true )
    {
    // Get the trigger delay
    Property prop;
    prop.type = TRIGGER_DELAY;
    error = this->Camera.GetProperty( &prop );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      }
    else
      {
      prop.onOff = true;
      prop.autoManualMode = false;
      // Set the frame rate.
      // Note that the actual recording frame rate may be slower,
      // depending on the bus speed and disk writing speed.
      prop.absValue = delay;
      error = this->Camera.SetProperty( &prop );
      if( error != PGRERROR_OK )
        {
        error.PrintErrorTrace();
        return;
        }
      }
    }
  this->delay = delay;
}


void CameraInput::SetCameraFrameRate( double frameRate )
{
  Error error;

  // Check if the camera supports the FRAME_RATE property
  std::cout << "Detecting frame rate from camera... " << std::endl;
  PropertyInfo propInfo;
  propInfo.type = FRAME_RATE;
  error = this->Camera.GetPropertyInfo( &propInfo );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return;
    }
  if( propInfo.present == true )
    {
      // Get the frame rate
    Property prop;
    prop.type = FRAME_RATE;
    error = this->Camera.GetProperty( &prop );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      }
    else
      {
      prop.autoManualMode = false;
      // Set the frame rate.
      // Note that the actual recording frame rate may be slower,
      // depending on the bus speed and disk writing speed.
      prop.absValue = frameRate;
      error = this->Camera.SetProperty( &prop );
      if( error != PGRERROR_OK )
        {
        error.PrintErrorTrace();
        return;
        }
      }
    }
  std::cout << "Asking frame rate of " << std::fixed << std::setprecision( 1 ) << frameRate << std::endl;
  this->GetCameraFrameRate();
}


double CameraInput::GetCameraFrameRate()
{
  Error error;

  // Check if the camera supports the FRAME_RATE property
  PropertyInfo propInfo;
  propInfo.type = FRAME_RATE;
  error = this->Camera.GetPropertyInfo( &propInfo );
  if( error != PGRERROR_OK )
    {
    error.PrintErrorTrace();
    return 0;
    }
  if( propInfo.present == true )
    {
    // Get the frame rate
    Property prop;
    prop.type = FRAME_RATE;
    error = this->Camera.GetProperty( &prop );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      }
    else
      {
      // Set the frame rate.
      // Note that the actual recording frame rate may be slower,
      // depending on the bus speed and disk writing speed.
      std::cout << "Using frame rate of " << std::fixed << std::setprecision( 1 ) << prop.absValue << std::endl;
      return prop.absValue;
      }
    }
  return 0;
}
// Note : Check the returned value when calling the function


void CameraInput::RecordImages()
{
  Error error;
  Image rawImage;
  for( int imageCount = 0; imageCount < this->NbImages; imageCount++ )
    {
      // Retrieve an image
    error = this->Camera.RetrieveBuffer( &rawImage );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      continue;
      }

    // Get the raw image dimensions
    PixelFormat pixFormat;
    unsigned int rows, cols, stride;
    rawImage.GetDimensions( &rows, &cols, &stride, &pixFormat );

    // Create a converted image
    Image convertedImage;

    // Convert the raw image
    error = rawImage.Convert( PIXEL_FORMAT_BGRU, &convertedImage );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      return;
      }

      // Get the camera information
    CameraInfo camInfo;
    error = this->Camera.GetCameraInfo( &camInfo );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      return;
      }
      // Create a unique filename
    std::ostringstream filename;
    filename << "Results\\" << camInfo.serialNumber << "-" << imageCount << ".bmp";

    // Save the image. If a file format is not passed in, then the file
    // extension is parsed to attempt to determine the file format.
    error = convertedImage.Save( filename.str().c_str() );
    if( error != PGRERROR_OK )
      {
      error.PrintErrorTrace();
      return;
      }
    }
  std::cout << std::endl;
  std::cout << "Finished grabbing images" << std::endl;
}
// note : return a value to detect an error ?


cv::Mat CameraInput::GetImageFromBuffer()
{
  FlyCapture2::Error error;
  static bool flag = false;
  FlyCapture2::Image rawImage;
  error = this->Camera.RetrieveBuffer( &rawImage );
  if( error != FlyCapture2::PGRERROR_OK )
    {
    error.PrintErrorTrace();
    //if (timer.elapsed() > warmup) { error_frame++; }
    //return;
    }
  //error_frame = 0;
  // convert to rgb
  FlyCapture2::Image rgbImage;
  rawImage.Convert( FlyCapture2::PIXEL_FORMAT_BGR, &rgbImage );

  // convert to OpenCV Mat
  cv::Mat mat = ConvertImageToMat( rgbImage );

  cv::transpose( mat, mat );
  //cv::flip( mat, mat, 0 );
  cv::transpose( mat, mat );
  //cv::flip( mat, mat, 0 );
  return mat;
}


void CameraInput::FindTopBottomLines( cv::Mat mat_color_ref, cv::Mat mat_color )
{
  if( !mat_color_ref.data || mat_color_ref.type() != CV_8UC3 || !mat_color.data || mat_color.type() != CV_8UC3 )
    {
    std::cout << "ERROR invalid cv::Mat data" << std::endl;
    return;
    }
  cv::Mat mat_BGR;
  cv::Mat mat_HSV;
  cv::Vec3b pixel_HSV;
  //int min_row = mat_color_ref.rows;
  //int max_row = 0;
  int max_i;
  int min_i;
  bool first = true;

  // Substract 2 images to keep only the line illuminated by the projector
  cv::subtract( mat_color, mat_color_ref, mat_BGR );
  if( !mat_BGR.data || mat_BGR.type() != CV_8UC3 )
    {
    std::cout << "ERROR invalid cv::Mat data\n" << std::endl;
    }

  //morphological opening (remove small objects from the foreground)
  cv::erode( mat_BGR, mat_BGR, cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( 5, 5 ) ) );
  dilate( mat_BGR, mat_BGR, cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( 5, 5 ) ) );

  //Convert the captured frame from BGR to HSV
  cv::cvtColor( mat_BGR, mat_HSV, cv::COLOR_BGR2HSV );

  max_i = 0;
  min_i = mat_HSV.rows;
  first = true;
  for( int i = 0; i < mat_HSV.rows; i++ )
    {
    for( int j = 0; j < mat_HSV.cols; j++ )
      {
      pixel_HSV = mat_HSV.at< cv::Vec3b >( i, j );
      if( pixel_HSV.val[ 2 ] > 90 )
        {
        if( first == true )
          {
          min_i = i;
          first = false;
          }
        max_i = i;
        }
      }
    }
  if( max_i > this->GetBottomLine() )
    {
    this->SetBottomLine( max_i );
    }
  if( min_i < this->GetTopLine() )
    {
    this->SetTopLine( min_i );
    }
}


void CameraInput::PutFrameInBuffer( cv::Mat &f, int index )
{
  int pos = index%this->BufferSize;
  this->FrameBuffer[ pos ] = f.clone();

  return;
}


cv::Mat CameraInput::ConvertImageToMat( FlyCapture2::Image rgbImage )
{
  unsigned int rowBytes = ( double )rgbImage.GetReceivedDataSize() / ( double )rgbImage.GetRows();
  cv::Mat mat = cv::Mat( rgbImage.GetRows(), rgbImage.GetCols(), CV_8UC3, rgbImage.GetData(), rowBytes );
  return mat;
}
