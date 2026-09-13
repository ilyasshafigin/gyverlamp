const exposes = require('zigbee-herdsman-converters/lib/exposes');

const e = exposes.presets;
const ea = exposes.access;

const analogInput = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const value = msg.data.presentValue;
        const endpoint = msg.endpoint.ID;

        if (typeof value !== 'number') {
            return;
}

if (endpoint >= 1 && endpoint <= 6) {
    return {[`button${endpoint}`]: value};
}

if (endpoint === 8) {
    return {voltage: value};
}
    },
};

const powerConfiguration = {
    cluster: 'genPowerCfg',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const data = msg.data;
        const result = {};

        const voltage =
            data.batteryVoltage ?? data[0x0020] ?? data['0x0020'];

        const battery =
            data.batteryPercentageRemaining ??
            data[0x0021] ??
            data['0x0021'];

        if (typeof voltage === 'number' && voltage !== 0xFF) {
            result.voltage = voltage / 10;
        }

        if (typeof battery === 'number' && battery !== 0xFF) {
            result.battery = battery / 2;
        }

        return Object.keys(result).length ? result : undefined;
    },
};

module.exports = {
    zigbeeModel: ['control_pad_6'],
    model: 'control_pad_6',
    vendor: 'ESPHome',
    description: 'ESPHome native Zigbee control pad',

    fromZigbee: [analogInput, powerConfiguration],
    toZigbee: [],

    exposes: [
        e.numeric('button1', ea.STATE).withLabel('Button1'),
        e.numeric('button2', ea.STATE).withLabel('Button2'),
        e.numeric('button3', ea.STATE).withLabel('Button3'),
        e.numeric('button4', ea.STATE).withLabel('Button4'),
        e.numeric('button5', ea.STATE).withLabel('Button5'),
        e.numeric('button6', ea.STATE).withLabel('Button6'),
        e.numeric('voltage', ea.STATE)
            .withLabel('Voltage')
            .withUnit('V')
            .withValueMin(0)
            .withValueMax(5),
        e.battery(),
    ],

    // Только initial read после pairing/reconfigure.
    configure: async (device) => {
        await device.getEndpoint(7).read('genPowerCfg', [0x0020, 0x0021]);
    },
};
