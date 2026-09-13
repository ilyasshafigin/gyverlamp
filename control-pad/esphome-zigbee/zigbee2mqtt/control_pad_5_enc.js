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

        if (endpoint === 7) {
            return {encoder: value};
        }
    },
};

module.exports = {
    zigbeeModel: ['control_pad_5_enc'],
    model: 'control_pad_5_enc',
    vendor: 'ESPHome',
    description: 'ESPHome native Zigbee control pad with EC11 encoder',

    fromZigbee: [analogInput],
    toZigbee: [],

    exposes: [
        e.numeric('button1', ea.STATE).withLabel('Button1').withValueMin(0).withValueMax(4).withValueStep(1),
        e.numeric('button2', ea.STATE).withLabel('Button2').withValueMin(0).withValueMax(4).withValueStep(1),
        e.numeric('button3', ea.STATE).withLabel('Button3').withValueMin(0).withValueMax(4).withValueStep(1),
        e.numeric('button4', ea.STATE).withLabel('Button4').withValueMin(0).withValueMax(4).withValueStep(1),
        e.numeric('button5', ea.STATE).withLabel('Button5').withValueMin(0).withValueMax(4).withValueStep(1),
        e.numeric('button6', ea.STATE).withLabel('Button6').withValueMin(0).withValueMax(4).withValueStep(1),
        // Encoder: -1 — против часовой стрелки, +1 — по часовой стрелке.
        e.numeric('encoder', ea.STATE).withLabel('Encoder').withValueMin(-1).withValueMax(1).withValueStep(1),
    ],
};
