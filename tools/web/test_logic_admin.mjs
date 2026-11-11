import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import {
    TIMEZONES, timezoneLabel, formatProgress, formatRssi, withKey, buildNetworkBody, buildDisplayBody,
} from './logic.mjs';

describe('admin helpers', () => {
    it('timezone presets include Paris and map back to labels', () => {
        assert.ok(TIMEZONES.some((t) => t.rule === 'CET-1CEST,M3.5.0,M10.5.0/3'));
        assert.equal(timezoneLabel('UTC0'), 'UTC');
        assert.equal(timezoneLabel('XYZ3'), 'Custom');
    });

    it('formats live progress', () => {
        assert.equal(formatProgress({ state: 'idle' }), null);
        assert.equal(formatProgress(undefined), null);
        assert.equal(formatProgress({ state: 'running', portions: 2, of: 5, g: '17.3' }), 'Dispensing 2/5 · 17.3 g');
        assert.equal(formatProgress({ state: 'weighing', portions: 5, of: 5, g: '40.0' }), 'Weighing…');
    });

    it('formats rssi quality', () => {
        assert.equal(formatRssi(-55), '-55 dBm · good');
        assert.equal(formatRssi(-65), '-65 dBm · fair');
        assert.equal(formatRssi(-80), '-80 dBm · weak');
        assert.equal(formatRssi(undefined), '—');
    });

    it('prefixes the web password', () => {
        assert.equal(withKey('{}', ''), '{}');
        assert.equal(withKey('{}', 'abcd'), 'key=abcd\n{}');
    });

    it('network body keeps only filled fields', () => {
        assert.deepEqual(buildNetworkBody({ ssid: 'Home', wifi_pass: '', mqtt_host: 'broker', mqtt_port: '1883', ntp: '' }),
            { ssid: 'Home', mqtt_host: 'broker', mqtt_port: 1883 });
        assert.deepEqual(buildNetworkBody({ ntp: 'fr.pool.ntp.org' }), { ntp: 'fr.pool.ntp.org' });
    });

    it('display body is numeric', () => {
        assert.deepEqual(buildDisplayBody({ mode: 'weight', brightness: '3', battery_off_s: '60', mains_off_s: '0' }),
            { mode: 'weight', brightness: 3, battery_off_s: 60, mains_off_s: 0 });
    });
});
