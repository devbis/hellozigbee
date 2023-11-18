# To run these tests install pytest, then run this command line:
# py.test -rfeEsxXwa --verbose --showlocals

import pytest
import time

from device import *
from zigbee import *
from conftest import *

EP3_ON = "SwitchEndpoint EP=3: do state change 1"
EP3_OFF = "SwitchEndpoint EP=3: do state change 0"
EP3_GET_STATE = "ZCL Read Attribute: EP=3 Cluster=0006 Command=00 Attr=0000"
EP3_SET_MODE = "ZCL Write Attribute: Clustter 0007 Attrib ff00"
EP3_GET_MODE = "ZCL Read Attribute: EP=3 Cluster=0007 Command=00 Attr=ff00"

def test_on_off(device, zigbee):    
    assert set_device_attribute(device, zigbee, 'state_button_2', 'ON', EP3_ON) == "ON"
    assert set_device_attribute(device, zigbee, 'state_button_2', 'OFF', EP3_OFF) == "OFF"


def test_toggle(device, zigbee):
    assert set_device_attribute(device, zigbee, 'state_button_2', 'OFF', EP3_OFF) == "OFF"
    assert get_device_attribute(device, zigbee, 'state_button_2', EP3_GET_STATE) == "OFF"

    assert set_device_attribute(device, zigbee, 'state_button_2', 'TOGGLE', EP3_ON) == "ON"
    assert get_device_attribute(device, zigbee, 'state_button_2', EP3_GET_STATE) == "ON"

    assert set_device_attribute(device, zigbee, 'state_button_2', 'TOGGLE', EP3_OFF) == "OFF"
    assert get_device_attribute(device, zigbee, 'state_button_2', EP3_GET_STATE) == "OFF"


def test_oosc_attributes(device, zigbee):
    assert set_device_attribute(device, zigbee, 'switch_mode_button_2', "toggle", EP3_SET_MODE) == "toggle"
    assert get_device_attribute(device, zigbee, 'switch_mode_button_2', EP3_GET_MODE) == "toggle"

    assert set_device_attribute(device, zigbee, 'switch_mode_button_2', "momentary", EP3_SET_MODE) == "momentary"
    assert get_device_attribute(device, zigbee, 'switch_mode_button_2', EP3_GET_MODE) == "momentary"

    assert set_device_attribute(device, zigbee, 'switch_mode_button_2', "multifunction", EP3_SET_MODE) == "multifunction"
    assert get_device_attribute(device, zigbee, 'switch_mode_button_2', EP3_GET_MODE) == "multifunction"