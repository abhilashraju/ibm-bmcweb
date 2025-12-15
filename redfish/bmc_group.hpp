// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#pragma once

#include "app.hpp"
#include "async_resp.hpp"
#include "dbus_utility.hpp"
#include "error_messages.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "logging.hpp"
#include "privileges.hpp"
#include "query.hpp"
#include "registries/privilege_registry.hpp"
#include "task.hpp"
#include "utils/dbus_utils.hpp"
#include "utils/json_utils.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url/format.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace redfish {

namespace bmc_group {
// D-Bus service and interface names for BMC group management
constexpr const char *bmcGroupService = "xyz.openbmc_project.BmcGroup.Manager";
constexpr const char *bmcGroupInterface =
    "xyz.openbmc_project.BmcGroup.Manager";
constexpr const char *bmcGroupObjectPath = "/xyz/openbmc_project/bmc_group";
constexpr const char *addToGroupMethod = "AddToGroup";
} // namespace bmc_group

/**
 * @brief Handler for monitoring the AddToGroup task progress
 *
 * This function is called when D-Bus signals are received during the
 * add-to-group operation. It updates the task state based on the operation
 * progress.
 */
inline bool
handleAddToGroupTask(const boost::system::error_code &ec,
                     sdbusplus::message_t &msg,
                     const std::shared_ptr<task::TaskData> &taskData) {
  if (ec) {
    BMCWEB_LOG_ERROR("Error in AddToGroup task: {}", ec.message());
    taskData->messages.emplace_back(messages::internalError());
    taskData->state = "Exception";
    taskData->status = "Critical";
    return task::completed;
  }

  // Parse the D-Bus signal to check operation status
  std::string interface;
  std::map<std::string, std::variant<std::string, bool, int>> properties;

  try {
    msg.read(interface, properties);
  } catch (const sdbusplus::exception_t &e) {
    BMCWEB_LOG_ERROR("Failed to parse D-Bus message: {}", e.what());
    return !task::completed;
  }

  // Check for Status property to determine task completion
  auto statusIt = properties.find("Status");
  if (statusIt != properties.end()) {
    const std::string *status = std::get_if<std::string>(&statusIt->second);
    if (status != nullptr) {
      BMCWEB_LOG_DEBUG("AddToGroup status: {}", *status);

      if (*status == "Completed") {
        taskData->messages.emplace_back(
            messages::taskCompletedOK(std::to_string(taskData->index)));
        taskData->state = "Completed";
        taskData->status = "OK";
        taskData->percentComplete = 100;
        return task::completed;
      } else if (*status == "Failed") {
        taskData->messages.emplace_back(messages::internalError());
        taskData->state = "Exception";
        taskData->status = "Critical";
        return task::completed;
      } else if (*status == "InProgress") {
        taskData->state = "Running";
        taskData->status = "OK";

        // Update progress if available
        auto progressIt = properties.find("Progress");
        if (progressIt != properties.end()) {
          const int *progress = std::get_if<int>(&progressIt->second);
          if (progress != nullptr) {
            taskData->percentComplete = *progress;
          }
        }
      }
    }
  }

  return !task::completed;
}

/**
 * @brief Handle POST request to add a BMC to the group
 *
 * This endpoint accepts a BMC name and initiates an asynchronous operation
 * to add the BMC to the group. It returns a task URI for monitoring progress.
 *
 * Request body format:
 * {
 *   "BmcName": "bmc-hostname-or-ip"
 * }
 *
 * Response includes:
 * - HTTP 202 Accepted
 * - Task URI in Location header
 * - TaskMonitor URI for progress tracking
 */
inline void handleBmcGroupAddToGroupPost(
    App &app, const crow::Request &req,
    const std::shared_ptr<bmcweb::AsyncResp> &asyncResp) {
  if (!redfish::setUpRedfishRoute(app, req, asyncResp)) {
    return;
  }

  BMCWEB_LOG_DEBUG("BmcGroup::AddToGroup POST");

  // Parse request body to get BMC name
  std::string bmcName;
  if (!json_util::readJsonPatch(req, asyncResp->res, "BmcName", bmcName)) {
    BMCWEB_LOG_ERROR("Failed to read BmcName from request");
    messages::propertyMissing(asyncResp->res, "BmcName");
    return;
  }

  if (bmcName.empty()) {
    BMCWEB_LOG_ERROR("BmcName is empty");
    messages::propertyValueFormatError(asyncResp->res, bmcName, "BmcName");
    return;
  }

  BMCWEB_LOG_INFO("Adding BMC '{}' to group", bmcName);

  // Create a task to monitor the add-to-group operation
  // The task will monitor D-Bus signals for operation progress
  std::string matchRule = "type='signal',"
                          "interface='org.freedesktop.DBus.Properties',"
                          "member='PropertiesChanged',"
                          "path='" +
                          std::string(bmc_group::bmcGroupObjectPath) + "'";

  std::shared_ptr<task::TaskData> task = task::TaskData::createTask(
      std::bind_front(handleAddToGroupTask), matchRule);

  // Set initial task state
  task->state = "Running";
  task->status = "OK";
  task->percentComplete = 0;
  task->messages.emplace_back(
      messages::taskStarted(std::to_string(task->index)));

  // Set task timeout to 5 minutes
  task->startTimer(std::chrono::minutes(5));

  // Create task payload with the request information
  task::Payload payload(req);
  payload.targetUri = "/redfish/v1/Oem/IBM/BmcGroup";
  task->payload.emplace(std::move(payload));

  // Populate response with task information
  // This sets HTTP 202 Accepted and includes TaskMonitor URI
  task->populateResp(asyncResp->res);

  // Initiate the D-Bus call to add BMC to group
  dbus::utility::async_method_call(
      [asyncResp, bmcName, task](const boost::system::error_code &ec) {
        if (ec) {
          BMCWEB_LOG_ERROR("D-Bus call to AddToGroup failed: {}", ec.message());
          task->state = "Exception";
          task->status = "Critical";
          task->messages.clear();
          task->messages.emplace_back(messages::internalError());
          return;
        }

        BMCWEB_LOG_INFO("AddToGroup D-Bus call initiated for BMC '{}'",
                        bmcName);
      },
      bmc_group::bmcGroupService, bmc_group::bmcGroupObjectPath,
      bmc_group::bmcGroupInterface, bmc_group::addToGroupMethod, bmcName);
}

/**
 * @brief Handle GET request for BMC Group information
 *
 * Returns information about the BMC Group resource including available actions.
 */
inline void
handleBmcGroupGet(App &app, const crow::Request &req,
                  const std::shared_ptr<bmcweb::AsyncResp> &asyncResp) {
  if (!redfish::setUpRedfishRoute(app, req, asyncResp)) {
    return;
  }

  BMCWEB_LOG_DEBUG("BmcGroup::GET");

  asyncResp->res.jsonValue["@odata.type"] = "#BmcGroup.v1_0_0.BmcGroup";
  asyncResp->res.jsonValue["@odata.id"] = "/redfish/v1/Oem/IBM/BmcGroup";
  asyncResp->res.jsonValue["Id"] = "BmcGroup";
  asyncResp->res.jsonValue["Name"] = "BMC Group Management";
  asyncResp->res.jsonValue["Description"] =
      "BMC Group Management Service for adding BMCs to groups";

  // Define available actions
  nlohmann::json &actions = asyncResp->res.jsonValue["Actions"];
  actions["#BmcGroup.AddToGroup"]["target"] =
      "/redfish/v1/Oem/IBM/BmcGroup/Actions/BmcGroup.AddToGroup";
  actions["#BmcGroup.AddToGroup"]["@Redfish.ActionInfo"] =
      "/redfish/v1/Oem/IBM/BmcGroup/AddToGroupActionInfo";
}

/**
 * @brief Handle GET request for AddToGroup action info
 *
 * Returns the action information describing parameters for AddToGroup action.
 */
inline void handleBmcGroupAddToGroupActionInfo(
    App &app, const crow::Request &req,
    const std::shared_ptr<bmcweb::AsyncResp> &asyncResp) {
  if (!redfish::setUpRedfishRoute(app, req, asyncResp)) {
    return;
  }

  BMCWEB_LOG_DEBUG("BmcGroup::AddToGroupActionInfo GET");

  asyncResp->res.jsonValue["@odata.type"] = "#ActionInfo.v1_1_2.ActionInfo";
  asyncResp->res.jsonValue["@odata.id"] =
      "/redfish/v1/Oem/IBM/BmcGroup/AddToGroupActionInfo";
  asyncResp->res.jsonValue["Id"] = "AddToGroupActionInfo";
  asyncResp->res.jsonValue["Name"] = "Add To Group Action Info";

  nlohmann::json &parameters = asyncResp->res.jsonValue["Parameters"];
  parameters = nlohmann::json::array();

  nlohmann::json parameter;
  parameter["Name"] = "BmcName";
  parameter["Required"] = true;
  parameter["DataType"] = "String";
  parameter["Description"] =
      "The hostname or IP address of the BMC to add to the group";
  parameters.push_back(std::move(parameter));
}

/**
 * @brief Register BMC Group routes
 *
 * Registers all Redfish routes for BMC Group management:
 * - GET /redfish/v1/Oem/IBM/BmcGroup
 * - POST /redfish/v1/Oem/IBM/BmcGroup/Actions/BmcGroup.AddToGroup
 * - GET /redfish/v1/Oem/IBM/BmcGroup/AddToGroupActionInfo
 */
inline void requestRoutesBmcGroup(App &app) {
  BMCWEB_ROUTE(app, "/redfish/v1/Oem/IBM/BmcGroup")
      .privileges(redfish::privileges::privilegeSetLogin)
      .methods(boost::beast::http::verb::get)(
          std::bind_front(handleBmcGroupGet, std::ref(app)));

  BMCWEB_ROUTE(app, "/redfish/v1/Oem/IBM/BmcGroup/Actions/BmcGroup.AddToGroup")
      .privileges(redfish::privileges::privilegeSetLogin)
      .methods(boost::beast::http::verb::post)(
          std::bind_front(handleBmcGroupAddToGroupPost, std::ref(app)));

  BMCWEB_ROUTE(app, "/redfish/v1/Oem/IBM/BmcGroup/AddToGroupActionInfo")
      .privileges(redfish::privileges::privilegeSetLogin)
      .methods(boost::beast::http::verb::get)(
          std::bind_front(handleBmcGroupAddToGroupActionInfo, std::ref(app)));
}

} // namespace redfish
