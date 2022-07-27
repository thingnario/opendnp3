/*
 * Copyright 2013-2019 Automatak, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Automatak
 * LLC (www.automatak.com) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Automatak LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "MasterTasks.h"
#include "openpal/logging/LogMacros.h"
#include "opendnp3/LogLevels.h"

#include "opendnp3/master/AssignClassTask.h"
#include "opendnp3/master/ClearRestartTask.h"
#include "opendnp3/master/DisableUnsolicitedTask.h"
#include "opendnp3/master/EnableUnsolicitedTask.h"
#include "opendnp3/master/EventScanTask.h"
#include "opendnp3/master/LANTimeSyncTask.h"
#include "opendnp3/master/SerialTimeSyncTask.h"
#include "opendnp3/master/StartupIntegrityPoll.h"

using namespace openpal;

namespace opendnp3
{

MasterTasks::MasterTasks(const MasterParams& params,
                         const openpal::Logger& logger,
                         IMasterApplication& app,
                         ISOEHandler& SOEHandler)
    : context(std::make_shared<TaskContext>()),
      clearRestart(std::make_shared<ClearRestartTask>(context, app, logger)),
      assignClass(std::make_shared<AssignClassTask>(context, app, RetryBehavior(params), logger)),
      startupIntegrity(std::make_shared<StartupIntegrityPoll>(
          context, app, SOEHandler, params.startupIntegrityClassMask, RetryBehavior(params), logger)),
      eventScan(std::make_shared<EventScanTask>(
          context, app, SOEHandler, params.eventScanOnEventsAvailableClassMask, logger)),
      // optional tasks
      disableUnsol(GetDisableUnsolTask(context, params, logger, app)),
      enableUnsol(GetEnableUnsolTask(context, params, logger, app)),
      timeSynchronization(GetTimeSyncTask(context, params.timeSyncMode, logger, app)),
      logger(logger)
{
    FORMAT_LOG_BLOCK(this->logger, opendnp3::flags::DBG, "MasterTasks::MasterTasks get called");
}

void MasterTasks::Initialize(IMasterScheduler& scheduler, IMasterTaskRunner& runner)
{
    FORMAT_LOG_BLOCK(this->logger, opendnp3::flags::DBG, "MasterTasks::Initialize get called");
    for (auto& task :
         {clearRestart, assignClass, startupIntegrity, eventScan, enableUnsol, disableUnsol, timeSynchronization})
    {
        if (task)
            scheduler.Add(task, runner);
    }

    for (auto& task : boundTasks)
    {
        scheduler.Add(task, runner);
    }
}

void MasterTasks::BindTask(const std::shared_ptr<IMasterTask>& task)
{
    boundTasks.push_back(task);
}

bool MasterTasks::DemandTimeSync()
{
    return this->Demand(this->timeSynchronization);
}

bool MasterTasks::DemandEventScan()
{
    return this->Demand(this->eventScan);
}

bool MasterTasks::DemandIntegrity()
{
    return this->Demand(this->startupIntegrity);
}

void MasterTasks::OnRestartDetected()
{
    FORMAT_LOG_BLOCK(logger, opendnp3::flags::DBG, "OnRestartDetected get called\n");
    this->Demand(this->clearRestart);
    this->Demand(this->assignClass);
    this->Demand(this->startupIntegrity);
    this->Demand(this->enableUnsol);
}

std::shared_ptr<IMasterTask> MasterTasks::GetTimeSyncTask(const std::shared_ptr<TaskContext>& context,
                                                          TimeSyncMode mode,
                                                          const openpal::Logger& logger,
                                                          IMasterApplication& application)
{
    switch (mode)
    {
    case (TimeSyncMode::NonLAN):
        return std::make_shared<SerialTimeSyncTask>(context, application, logger);
    case (TimeSyncMode::LAN):
        return std::make_shared<LANTimeSyncTask>(context, application, logger);
    default:
        return nullptr;
    }
}

std::shared_ptr<IMasterTask> MasterTasks::GetEnableUnsolTask(const std::shared_ptr<TaskContext>& context,
                                                             const MasterParams& params,
                                                             const openpal::Logger& logger,
                                                             IMasterApplication& application)
{
    return params.unsolClassMask.HasEventClass()
        ? std::make_shared<EnableUnsolicitedTask>(context, application, RetryBehavior(params), params.unsolClassMask,
                                                  logger)
        : nullptr;
}

std::shared_ptr<IMasterTask> MasterTasks::GetDisableUnsolTask(const std::shared_ptr<TaskContext>& context,
                                                              const MasterParams& params,
                                                              const openpal::Logger& logger,
                                                              IMasterApplication& application)
{
    return params.disableUnsolOnStartup
        ? std::make_shared<DisableUnsolicitedTask>(
              context, application,
              TaskBehavior::SingleImmediateExecutionWithRetry(params.taskRetryPeriod, params.maxTaskRetryPeriod),
              logger)
        : nullptr;
}

} // namespace opendnp3
