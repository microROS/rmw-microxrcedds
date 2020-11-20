// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rmw_uros/options.h"

#include "types.h"

#include <rmw_microxrcedds_c/config.h>
#include <rmw/allocators.h>
#include <rmw/ret_types.h>
#include <rmw/error_handling.h>

#include <uxr/client/client.h>

rmw_ret_t rmw_uros_init_options(
  int argc, const char * const argv[],
  rmw_init_options_t * rmw_options)
{
  if (NULL == rmw_options) {
    RMW_SET_ERROR_MSG("Uninitialised rmw_init_options.");
    return RMW_RET_INVALID_ARGUMENT;
  }
  rmw_ret_t ret = RMW_RET_OK;
  // TODO(pablogs9): Is the impl allocated at this point?
  // rmw_options->impl = rmw_options->allocator.allocate(
  // sizeof(rmw_init_options_impl_t),
  // rmw_options->allocator.state);
#if defined(MICRO_XRCEDDS_SERIAL) || defined(MICRO_XRCEDDS_CUSTOM_SERIAL)
  if (argc >= 2) {
    strcpy(rmw_options->impl->connection_params.serial_device, argv[1]);
  } else {
    RMW_SET_ERROR_MSG(
      "Wrong number of arguments in rmw options. Needs one argument with the serial device.");
    ret = RMW_RET_INVALID_ARGUMENT;
  }

#elif defined(MICRO_XRCEDDS_UDP)
  if (argc >= 3) {
    strcpy(rmw_options->impl->connection_params.agent_address, argv[1]);
    strcpy(rmw_options->impl->connection_params.agent_port, argv[2]);
  } else {
    RMW_SET_ERROR_MSG("Wrong number of arguments in rmw options. Needs an Agent IP and port.");
    ret = RMW_RET_INVALID_ARGUMENT;
  }
#else
  (void) argc;
  (void) argv;
#endif
  return ret;
}

rmw_ret_t rmw_uros_options_set_serial_device(const char * dev, rmw_init_options_t * rmw_options)
{
#if defined(MICRO_XRCEDDS_SERIAL) || defined(MICRO_XRCEDDS_CUSTOM_SERIAL)
  if (NULL == rmw_options) {
    RMW_SET_ERROR_MSG("Uninitialised rmw_init_options.");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (dev != NULL && strlen(dev) <= MAX_SERIAL_DEVICE) {
    strcpy(rmw_options->impl->connection_params.serial_device, dev);
  } else {
    RMW_SET_ERROR_MSG("serial port configuration error");
    return RMW_RET_INVALID_ARGUMENT;
  }
  return RMW_RET_OK;
#else
  (void) dev;
  (void) rmw_options;

  RMW_SET_ERROR_MSG("MICRO_XRCEDDS_SERIAL not set.");
  return RMW_RET_INVALID_ARGUMENT;
#endif
}

rmw_ret_t rmw_uros_options_set_udp_address(
  const char * ip, const char * port,
  rmw_init_options_t * rmw_options)
{
#ifdef MICRO_XRCEDDS_UDP
  if (NULL == rmw_options) {
    RMW_SET_ERROR_MSG("Uninitialised rmw_init_options.");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (ip != NULL && strlen(ip) <= MAX_IP_LEN) {
    strcpy(rmw_options->impl->connection_params.agent_address, ip);
  } else {
    RMW_SET_ERROR_MSG("default ip configuration error");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (port != NULL && strlen(port) <= MAX_PORT_LEN) {
    strcpy(rmw_options->impl->connection_params.agent_port, port);
  } else {
    RMW_SET_ERROR_MSG("default port configuration error");
    return RMW_RET_INVALID_ARGUMENT;
  }

  return RMW_RET_OK;
#else
  (void) ip;
  (void) port;
  (void) rmw_options;

  RMW_SET_ERROR_MSG("MICRO_XRCEDDS_UDP not set.");
  return RMW_RET_INVALID_ARGUMENT;
#endif
}

#if defined(MICRO_XRCEDDS_UDP) && defined(UCLIENT_PROFILE_DISCOVERY)
bool on_agent_found(const TransportLocator* locator, void* args)
{
  rmw_init_options_t * rmw_options = (rmw_init_options_t *) args;
  uxrIpProtocol ip_protocol;
  char ip[MAX_IP_LEN];
  char port_str[MAX_PORT_LEN];
  uint16_t port;

  uxr_locator_to_ip(locator, ip, sizeof(ip), &port, &ip_protocol);
  sprintf(port_str, "%d", port);

  uxrUDPTransport transport;
  uxrUDPPlatform udp_platform;
  if(uxr_init_udp_transport(&transport, &udp_platform, ip_protocol, ip, port_str))
  {
    uxrSession session;
    uxr_init_session(&session, &transport.comm, rmw_options->impl->connection_params.client_key);
    if(uxr_create_session_retries(&session, 5))
    {
      sprintf(rmw_options->impl->connection_params.agent_port, "%d", port);
      sprintf(rmw_options->impl->connection_params.agent_address, "%s", ip);
      uxr_delete_session(&session);
      return true;
    }
  }
  return false;
}
#endif

rmw_ret_t rmw_uros_discover_agent(rmw_init_options_t * rmw_options)
{
#if defined(MICRO_XRCEDDS_UDP) && defined(UCLIENT_PROFILE_DISCOVERY)
  if (NULL == rmw_options) {
    RMW_SET_ERROR_MSG("Uninitialised rmw_init_options.");
    return RMW_RET_INVALID_ARGUMENT;
  }

  memset(rmw_options->impl->connection_params.agent_address, 0, MAX_IP_LEN);
  memset(rmw_options->impl->connection_params.agent_port, 0, MAX_PORT_LEN);

  uxr_discovery_agents_default(1, 1000, on_agent_found, (void*) rmw_options);

  return (strlen(rmw_options->impl->connection_params.agent_address) > 0 )? RMW_RET_OK : RMW_RET_TIMEOUT;
#else
  (void) rmw_options;

  RMW_SET_ERROR_MSG("MICRO_XRCEDDS_UDP or UCLIENT_PROFILE_DISCOVERY not set.");
  return RMW_RET_INVALID_ARGUMENT;
#endif
}

rmw_ret_t rmw_uros_options_set_client_key(uint32_t client_key, rmw_init_options_t * rmw_options)
{
  if (NULL == rmw_options) {
    RMW_SET_ERROR_MSG("Uninitialised rmw_init_options.");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_options->impl->connection_params.client_key = client_key;

  return RMW_RET_OK;
}

rmw_ret_t rmw_uros_check_agent_status(int timeout_ms)
{  
  bool synchronized = true;
  rmw_uxrce_mempool_item_t * item = session_memory.allocateditems;
  while (item != NULL) {
    rmw_context_impl_t * context = (rmw_context_impl_t *) item->data;
    synchronized &= uxr_sync_session(&context->session, timeout_ms);
    item = item->next;
  }
  return (synchronized && session_memory.allocateditems != NULL) ? RMW_RET_OK : RMW_RET_ERROR;
}
