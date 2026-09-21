"use strict";

const assert = require("assert");
const crypto = require("crypto");

const pairKey = Buffer.from("000102030405060708090A0B0C0D0E0F", "hex");
const panelMac = Buffer.from("020000000001", "hex");
const lampMac = Buffer.from("020000000002", "hex");
const bootNonce = Buffer.from("404142434445464748494A4B4C4D4E4F", "hex");

function hmac(key, data) {
  return crypto.createHmac("sha256", key).update(data).digest();
}

const commandKey = hmac(
  pairKey,
  Buffer.concat([
    Buffer.from("GyverLamp/control-pad/command/v1", "ascii"),
    Buffer.from([0]),
    panelMac,
    lampMac,
  ])
);
assert.strictEqual(
  commandKey.toString("hex").toUpperCase(),
  "4D5E21321524733F8BE0C8293515A39E2A26C445C7B9D4F1454A6A6BFC3B7621"
);

const commandHeader = Buffer.from("4750010727000100", "hex");
const commandPayload = Buffer.concat([
  bootNonce,
  Buffer.from([0x04, 0x03, 0x02, 0x01, 0x06, 0x02, 0xfc]),
]);
const command = Buffer.concat([commandHeader, commandPayload, hmac(commandKey, Buffer.concat([commandHeader, commandPayload])).subarray(0, 16)]);
assert.strictEqual(
  command.toString("hex").toUpperCase(),
  "4750010727000100404142434445464748494A4B4C4D4E4F040302010602FC1C79049A1CAE16327546F16DD7BC34A1"
);

const ackHeader = Buffer.from("4750010825000100", "hex");
const ackPayload = Buffer.concat([
  bootNonce,
  Buffer.from([0x04, 0x03, 0x02, 0x01, 0x01]),
]);
const ack = Buffer.concat([ackHeader, ackPayload, hmac(commandKey, Buffer.concat([ackHeader, ackPayload])).subarray(0, 16)]);
assert.strictEqual(
  ack.toString("hex").toUpperCase(),
  "4750010825000100404142434445464748494A4B4C4D4E4F04030201018C95915997786CEC5DA63439032C9791"
);

console.log("Node command KAT: passed");
