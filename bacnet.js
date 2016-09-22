const addon = require('./build/Release/binding.node')
const bacnet = Object.create(addon)
const EventEmitter = require('events').EventEmitter
const util = require('util')

function BACnetInstance (config) {
  EventEmitter.call(this)
  const bacnetAddon = addon.init(flattenConfig(config))
  bacnetAddon.initClient(this)
  if (config && config.device) bacnetAddon.initDevice()
  bacnetAddon.listen()

  const confirmedCallbacks = {}
  setupMethods.call(this, bacnetAddon, confirmedCallbacks)
  setupHandlers.call(this, confirmedCallbacks)
}

function setupMethods (bacnetAddon, confirmedCallbacks) {
  function addCallback (invokeId, callback) {
    if (callback && invokeId > 0) {
      if (typeof callback !== 'function') throw new TypeError('non-function passed as callback argument')
      confirmedCallbacks[invokeId] = callback
    }
    return invokeId
  }

  this.closeQueue = bacnetAddon.closeQueue
  this.whois = bacnetAddon.whois
  this.isBound = bacnetAddon.isBound
  this.readProperty = function (deviceInstance, objectType, objectInstance, property, arrayIndex, callback) {
    if (!objectType) throw new TypeError('Expected an object type, got : ' + objectType)
    const invokeId = bacnetAddon.readProperty(deviceInstance, bacnet.objectTypeToNumber(objectType), objectInstance, bacnet.propertyKeyToNumber(property), arrayIndex)
    if (invokeId === 0) throw new Error('Invoking BACnet read failed')
    return addCallback(invokeId, callback)
  }
  this.writeProperty = function (deviceInstance, objectType, objectInstance, property, arrayIndex, value, priority, callback) {
    if (!objectType) throw new TypeError('Expected an object type, got : ' + objectType)
    if (value.constructor !== bacnet.BacnetValue) {
      value = new bacnet.BacnetValue(value)
    }
    if (!priority) priority = 15
    const invokeId = bacnetAddon.writeProperty(deviceInstance, bacnet.objectTypeToNumber(objectType), objectInstance, bacnet.propertyKeyToNumber(property), arrayIndex, value, priority)
    if (invokeId === 0) throw new Error('Invoking BACnet write failed')
    return addCallback(invokeId, callback)
  }
}

function setupHandlers (confirmedCallbacks) {
  function executeCallback () {
    const invocationCallback = confirmedCallbacks[ this ]
    if (invocationCallback) {
      delete confirmedCallbacks[ this ]
      try {
        invocationCallback.apply(null, arguments)
      } catch (err) {
        console.log('Error in callback', err.stack)
        this.emit('error', err)
      }
    }
  }

  this.on('ack', function onAck (invokeId, response) {
    executeCallback.call(invokeId, null, response)
  })

  this.on('abort', function onAbort (invokeId, reason) {
    console.log('abort', invokeId)
    executeCallback.call(invokeId, new Error(reason))
  })

  this.on('reject', function onReject (invokeId, reason) {
    console.log('abort', invokeId)
    executeCallback.call(invokeId, new Error(reason))
  })

  this.on('error-ack', function onErrorAck (invokeId, error) {
    console.log('error-ack', invokeId, error)
    executeCallback.call(invokeId, new Error('Error received in acknowledgment for request #' + invokeId + ' ' + error[ 'error-class' ] + '/' + error[ 'error-code' ]))
  })
}

function flattenConfig (config) {
  var flatConfig = config && config.datalink || {} // I've flattened the config as I had trouble getting nested properties in the c++
  flatConfig.device_instance_id = config.device_instance_id
  return flatConfig
}

util.inherits(BACnetInstance, EventEmitter)

/**
 * {
 *   datalink: {
 *     iface: process.env.BACNET_INTERFACE,
 *     ip_port: process.env.BACNET_PORT || 0xBAC0
 *   },
 *   device: false,
 *   device_instance_id: 12
 * }
 * @param config
 * @returns {*|EventEmitter}
 */
bacnet.init = function init (config) {
  return new BACnetInstance(config)
}

module.exports = bacnet
