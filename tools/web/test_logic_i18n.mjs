import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { tr, pickLang, errorWords, formatNextFeedParts } from './logic.mjs';

describe('i18n', () => {
    it('picks the stored language, else the browser', () => {
        assert.equal(pickLang('fr', 'en-US'), 'fr');
        assert.equal(pickLang('', 'fr-FR'), 'fr');
        assert.equal(pickLang(undefined, 'de'), 'en');
    });
    it('translates labels, built strings and problem text', () => {
        assert.equal(tr('Home', 'en'), 'Home');
        assert.equal(tr('Home', 'fr'), 'Accueil');
        assert.equal(tr(formatNextFeedParts({ hour: 9, min: 0, g: 20, in_min: 15 }).headline, 'fr'), 'Prochain repas · dans 15 min');
        assert.equal(tr('Feed failed (stuck); Bowl missing at feed', 'fr'), 'Repas raté (bloqué) ; Gamelle absente au repas');
        assert.equal(tr('Connection lost (12 min)', 'fr'), 'Connexion perdue (12 min)');
        assert.equal(tr('Something new', 'fr'), 'Something new');
        assert.equal(tr(errorWords('no_meal_today'), 'fr'), 'plus de repas prévu aujourd’hui');
    });
});
