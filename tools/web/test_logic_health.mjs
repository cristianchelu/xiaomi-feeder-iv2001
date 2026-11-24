import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { formatHistoryRow, formatUptime, buildFeedLimitBody, slotStateTone } from './logic.mjs';

describe('health helpers', () => {
    it('formats a stuck feed with its reason', () => {
        const row = formatHistoryRow({ t: 0, src: 's', want: 30, got: 0, meas: 1, res: 'stuck', por: 3, pul: 1, aj: 3, why: 'antijam', jam: 'stall', ok: 40, busy: 2, err: 0, ma: 282, dur: 31, mqtt: 0, bowl: 1 });
        assert.equal(row.source, 'Schedule');
        assert.equal(row.tone, 'bad');
        assert.equal(row.got, '0 g');
        assert.match(row.detail, /1\/3 holes/);
        assert.match(row.detail, /jam not cleared after 3 tries/);
        assert.match(row.detail, /last jam: stall/);
        assert.match(row.detail, /2 sensor reads refused/);
        assert.match(row.detail, /offline/);
    });

    it('formats an ok estimated feed', () => {
        const row = formatHistoryRow({ t: 0, src: 'm', want: 0, got: 20, meas: 0, res: 'ok', por: 2, pul: 2, aj: 0, why: '-', jam: '-', ok: 10, busy: 0, err: 0, ma: 120, dur: 13, mqtt: 1, bowl: 1 });
        assert.equal(row.tone, 'ok');
        assert.equal(row.want, '2 portion(s)');
        assert.equal(row.got, '20 g (est.)');
    });

    it('formats uptime and the limit body', () => {
        assert.equal(formatUptime(59), '0 min');
        assert.equal(formatUptime(3720), '1 h 2 min');
        assert.equal(formatUptime(90000), '1 d 1 h');
        assert.deepEqual(buildFeedLimitBody({ daily_max_g: '200', not_eaten_h: '' }), { daily_max_g: 200, not_eaten_h: 0 });
    });

    it('failed slots are red, limit skips orange', () => {
        assert.equal(slotStateTone('failed'), 'bad');
        assert.equal(slotStateTone('skipped_limit'), 'warn');
    });
});
