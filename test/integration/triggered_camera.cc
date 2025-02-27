/*
 * Copyright (C) 2022 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <gtest/gtest.h>

#include <ignition/common/Console.hh>
#include <ignition/common/Filesystem.hh>
#include <ignition/sensors/CameraSensor.hh>
#include <ignition/sensors/Manager.hh>

// TODO(louise) Remove these pragmas once ign-rendering is disabling the
// warnings
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4251)
#endif
#include <ignition/rendering/RenderEngine.hh>
#include <ignition/rendering/RenderingIface.hh>
#include <ignition/rendering/Scene.hh>
#ifdef _WIN32
#pragma warning(pop)
#endif

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4005)
#pragma warning(disable: 4251)
#endif
#include <ignition/msgs.hh>
#ifdef _WIN32
#pragma warning(pop)
#endif

#include "test_config.h"  // NOLINT(build/include)
#include "TransportTestTools.hh"

using namespace std::chrono_literals;

class CameraSensorTest: public testing::Test,
  public testing::WithParamInterface<const char *>
{
  // Documentation inherited
  protected: void SetUp() override
  {
    ignition::common::Console::SetVerbosity(4);
  }

  // Create a Camera sensor from a SDF and gets a image message
  public: void ImagesWithBuiltinSDF(const std::string &_renderEngine);
};

void CameraSensorTest::ImagesWithBuiltinSDF(const std::string &_renderEngine)
{
  std::string path = ignition::common::joinPaths(PROJECT_SOURCE_PATH, "test",
      "sdf", "triggered_camera_sensor_builtin.sdf");
  sdf::SDFPtr doc(new sdf::SDF());
  sdf::init(doc);
  ASSERT_TRUE(sdf::readFile(path, doc));
  ASSERT_NE(nullptr, doc->Root());
  ASSERT_TRUE(doc->Root()->HasElement("model"));
  auto modelPtr = doc->Root()->GetElement("model");
  ASSERT_TRUE(modelPtr->HasElement("link"));
  auto linkPtr = modelPtr->GetElement("link");
  ASSERT_TRUE(linkPtr->HasElement("sensor"));
  auto sensorPtr = linkPtr->GetElement("sensor");

  // If ogre is not the engine, don't run the test
  if (_renderEngine.compare("ogre") != 0)
  {
    igndbg << "Engine '" << _renderEngine
      << "' doesn't support segmentation cameras" << std::endl;
    return;
  }

  // Setup ign-rendering with an empty scene
  auto *engine = ignition::rendering::engine(_renderEngine);
  if (!engine)
  {
    igndbg << "Engine '" << _renderEngine
           << "' is not supported" << std::endl;
    return;
  }

  ignition::rendering::ScenePtr scene = engine->CreateScene("scene");

  ignition::sensors::Manager mgr;

  ignition::sensors::CameraSensor *sensor =
      mgr.CreateSensor<ignition::sensors::CameraSensor>(sensorPtr);
  ASSERT_NE(sensor, nullptr);
  sensor->SetScene(scene);

  sdf::Sensor sdfSensor;
  sdfSensor.Load(sensorPtr);
  EXPECT_EQ(true, sdfSensor.CameraSensor()->Triggered());

  ASSERT_NE(sensor->RenderingCamera(), nullptr);
  EXPECT_NE(sensor->Id(), sensor->RenderingCamera()->Id());
  EXPECT_EQ(256u, sensor->ImageWidth());
  EXPECT_EQ(257u, sensor->ImageHeight());

  // check camera image before trigger
  {
    std::string imageTopic =
        "/test/integration/TriggeredCameraPlugin_imagesWithBuiltinSDF";
    WaitForMessageTestHelper<ignition::msgs::Image> helper(imageTopic);
    mgr.RunOnce(std::chrono::steady_clock::duration::zero(), true);
    EXPECT_FALSE(helper.WaitForMessage(3s)) << helper;
  }

  // trigger camera through topic
  ignition::transport::Node triggerNode;
  std::string triggerTopic =
      "/test/integration/TriggeredCameraPlugin_imagesWithBuiltinSDF/trigger";

  auto pub = triggerNode.Advertise<ignition::msgs::Boolean>(triggerTopic);
  ignition::msgs::Boolean msg;
  msg.set_data(true);
  pub.Publish(msg);

  // check camera image after trigger
  {
    std::string imageTopic =
        "/test/integration/TriggeredCameraPlugin_imagesWithBuiltinSDF";
    WaitForMessageTestHelper<ignition::msgs::Image> helper(imageTopic);
    mgr.RunOnce(std::chrono::steady_clock::duration::zero(), true);
    EXPECT_TRUE(helper.WaitForMessage(3s)) << helper;
  }

  // test removing sensor
  // first make sure the sensor objects do exist
  auto sensorId = sensor->Id();
  auto cameraId = sensor->RenderingCamera()->Id();
  EXPECT_EQ(sensor, mgr.Sensor(sensorId));
  EXPECT_EQ(sensor->RenderingCamera(), scene->SensorById(cameraId));
  // remove and check sensor objects no longer exist in both sensors and
  // rendering
  EXPECT_TRUE(mgr.Remove(sensorId));
  EXPECT_EQ(nullptr, mgr.Sensor(sensorId));
  EXPECT_EQ(nullptr, scene->SensorById(cameraId));

  // Clean up
  engine->DestroyScene(scene);
  ignition::rendering::unloadEngine(engine->Name());
}

//////////////////////////////////////////////////
TEST_P(CameraSensorTest, ImagesWithBuiltinSDF)
{
  ImagesWithBuiltinSDF(GetParam());
}

INSTANTIATE_TEST_CASE_P(CameraSensor, CameraSensorTest,
    RENDER_ENGINE_VALUES, ignition::rendering::PrintToStringParam());

//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ignition::common::Console::SetVerbosity(4);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
