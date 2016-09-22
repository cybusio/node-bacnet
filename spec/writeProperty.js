/* global describe, it, before, after */

require('should')
const bacnet = require('../bacnet.js')
const tools = require('./tools')

const iface = tools.getSuitableBroadcastInterface()

describe('Write property', function () {
  var device
  after('Exit the device fork', function (done) {
    device.once('exit', done)
    device.exit()
  })
  before(function (done) {
    device = tools.deviceProcess({
      datalink: {
        iface: iface
      },
      device: true
    })
    device.once('up', done)
  })
  // my writes dont currently support arrays - only single values
  describe('writing its own device object', function () {
    // writeAndReadOwnDeviceObjectPropertyTest('object-identifier', {type: 'device', instance: 262626})
    writeAndReadOwnDeviceObjectPropertyTest('object-name', 'New device name')
    writeAndReadOwnDeviceObjectPropertyTest('description', 'This is a server')
    // readOwnDeviceObjectPropertyTest('system-status', ['operational'])
    // writeAndReadOwnDeviceObjectPropertyTest('vendor-name', 'Relayr')
    writeAndReadOwnDeviceObjectPropertyTest('vendor-identifier', 1)
    writeAndReadOwnDeviceObjectPropertyTest('model-name', 'Test BACnet thing')
    // readOwnDeviceObjectPropertyTest('firmware-revision', ['0.9.0'])
    // readOwnDeviceObjectPropertyTest('application-software-version', ['1.0'])
    writeAndReadOwnDeviceObjectPropertyTest('location', 'Germany')
    // // readOwnDeviceObjectPropertyTest('local-time', [])
    // // readOwnDeviceObjectPropertyTest('local-date', [])
    // // readOwnDeviceObjectPropertyTest('utc-offset', [])
    // // readOwnDeviceObjectPropertyTest('daylight-savings-status', [])
    // readOwnDeviceObjectPropertyTest('protocol-version', [1])
    // readOwnDeviceObjectPropertyTest('protocol-revision', [14])
    // // readOwnDeviceObjectPropertyTest('protocol-services-supported', [])
    // readOwnDeviceObjectPropertyTest('object-list', [{type: 'device', instance: 260001}])
    // writeAndReadOwnDeviceObjectPropertyTest('max-apdu-length-accepted', 1000)
    // readOwnDeviceObjectPropertyTest('segmentation-supported', ['no-segmentation'])
    writeAndReadOwnDeviceObjectPropertyTest('apdu-timeout', 4000)
    writeAndReadOwnDeviceObjectPropertyTest('number-of-apdu-retries', 5)
    // readOwnDeviceObjectPropertyTest('database-revision', [0])
    function writeAndReadOwnDeviceObjectPropertyTest (propertyKey, value) {
      it('can write the \'' + propertyKey + '\' property', function (done) {
        device.once('read-property-ack', (property) => {
          property.object.should.deepEqual({type: 'device', instance: 260001})
          property.property.should.equal(bacnet.propertyKeyToNumber(propertyKey))
          property.value.should.deepEqual([value])
          done()
        })
        device.once('write-property-ack', () => {
          device.readProperty('127.0.0.1', 'device', 260001, propertyKey, false)
        })
        device.writeProperty('127.0.0.1', 'device', 260001, propertyKey, false, value, 8)
        device.once('error', done)
      })
    }
  })
})
