/* Pure helpers — spec/30-processes/web-ui-client.md */

export const DAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
export const ALL_DAYS_MASK = 127;

const HOPPER = { empty: ['Empty', 'bad'], low: ['Low', 'warn'], normal: ['Normal', 'ok'] };
const SLOT_TONE = {
    dispensed: 'ok',
    pending: 'warn',
    to_be_skipped: 'warn',
    missed: 'bad',
    skipped: 'muted',
    skipped_full: 'muted',
    skipped_limit: 'warn',
    failed: 'bad',
};
const SKIP_HIDE = new Set(['dispensed', 'failed', 'skipped_full', 'skipped_limit', 'dispensing', 'skipped']);

export function indicesToMask(indices) {
    let mask = 0;
    for (const i of indices) mask |= 1 << i;
    return mask;
}

export function maskToIndices(mask) {
    const out = [];
    for (let i = 0; i < 7; i++) if (mask & (1 << i)) out.push(i);
    return out;
}

export function readMaskFromCheckedValues(values) {
    let mask = 0;
    for (const v of values) mask |= 1 << Number(v);
    return mask;
}

export function validateScheduleMask(mask) {
    return mask ? { ok: true } : { ok: false, error: 'Pick at least one day' };
}

function schedTime(hour, min) {
    return { hour: Number(hour), min: Number(min) };
}

export function buildScheduleSetBody({ hour, min, mask, g, enabled }) {
    return { ...schedTime(hour, min), repeat_days: maskToIndices(mask), g: Number(g), enabled: Boolean(enabled) };
}

export const buildScheduleDeleteBody = schedTime;
export const buildScheduleToggleBody = schedTime;

export function buildScheduleSkipBody(hour, min, skip = true) {
    return { ...schedTime(hour, min), skip: Boolean(skip) };
}

export function buildDispenseBody(g) {
    return { g: Number(g) };
}

export function buildScheduleEnableBody(enabled) {
    return { enabled: Boolean(enabled) };
}

export function buildScheduleTodayBody(enabled) {
    return { enabled: Boolean(enabled) };
}

export function buildConfigTzBody(tz_rule) {
    return { tz_rule };
}

export function buildFeedOverfillBody({ enabled, threshold_g }) {
    const body = {};
    if (enabled !== undefined) body.enabled = Boolean(enabled);
    if (threshold_g !== undefined && threshold_g !== '') body.threshold_g = Number(threshold_g);
    return body;
}

export function formatNextFeedParts(next) {
    if (!next) return { headline: 'Next feed · nothing scheduled', detail: null };
    const hh = String(next.hour).padStart(2, '0');
    const mm = String(next.min).padStart(2, '0');
    const when = next.in_min === 0 ? 'now' : `in ${next.in_min} min`;
    return { headline: `Next feed · ${when}`, detail: `${hh}:${mm} · ${next.g}g` };
}

export function sortSlotsByTime(slots) {
    return [...slots].sort((a, b) => {
        const [ah, am] = a.time.split(':').map(Number);
        const [bh, bm] = b.time.split(':').map(Number);
        return ah !== bh ? ah - bh : am - bm;
    });
}

export function formatStatusAlerts(st) {
    const out = [];
    if (!st.time_synced) out.push({ text: 'Time not synced', tone: 'warn' });
    if (st.bowl_error) out.push({ text: 'Bowl problem', tone: 'bad' });
    return out;
}

function wireText(wire, empty, unknown) {
    if (wire === undefined) return '—';
    if (wire === '') return unknown;
    return `${wire} g`;
}

function wireTone(wire, emptyTone = 'muted') {
    if (wire === undefined) return emptyTone;
    if (wire === '') return 'warn';
    return 'ok';
}

export function formatBowlWeightWire(wire) {
    return wireText(wire, '—', 'Unknown');
}

export function bowlWeightTone(wire) {
    return wireTone(wire);
}

export function formatHopperLevel(hopper) {
    return (HOPPER[hopper] || HOPPER.normal)[0];
}

export function hopperTone(hopper) {
    return (HOPPER[hopper] || HOPPER.normal)[1];
}

export function formatBowlHealth(bowl_error) {
    if (bowl_error === undefined) return '—';
    return bowl_error ? 'Problem' : 'OK';
}

export function bowlHealthTone(bowl_error) {
    if (bowl_error === undefined) return 'muted';
    return bowl_error ? 'bad' : 'ok';
}

export function formatBatteryWire(wire) {
    if (wire === undefined) return '—';
    if (wire === 'unknown') return 'Unknown';
    return `${wire}%`;
}

export function formatMainsWire(wire) {
    if (wire === undefined) return '—';
    return wire === 'ON' ? 'Mains' : 'Battery';
}

export function formatBusy(busy) {
    return busy ? 'Feeding' : 'Idle';
}

export function busyTone(busy) {
    return busy ? 'warn' : 'ok';
}

export function statusStateRows(st) {
    const rows = [
        { label: 'Food in bowl', value: formatBowlWeightWire(st.bowl_weight), tone: bowlWeightTone(st.bowl_weight) },
        { label: 'Hopper', value: formatHopperLevel(st.hopper), tone: hopperTone(st.hopper) },
        { label: 'Activity', value: formatBusy(st.dispense_busy), tone: busyTone(st.dispense_busy) },
        { label: 'Bowl', value: formatBowlHealth(st.bowl_error), tone: bowlHealthTone(st.bowl_error) },
    ];
    if (st.battery !== undefined) {
        rows.push({ label: 'Battery', value: formatBatteryWire(st.battery), tone: st.battery === 'unknown' ? 'warn' : 'ok' });
    }
    if (st.mains !== undefined) {
        rows.push({ label: 'Power', value: formatMainsWire(st.mains), tone: st.mains === 'ON' ? 'ok' : 'muted' });
    }
    return rows;
}

export function formatDeviceTime(st) {
    return st.time_synced && st.local_time ? st.local_time : '—';
}

export function formatSlotClock(time) {
    const [h, m] = String(time).split(':');
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
}

export function formatSlotSummary(time, g) {
    return `${formatSlotClock(time)} · ${g}g`;
}

export function weekdayFromLocalTime(local_time) {
    if (!local_time || local_time.length < 10) return null;
    const parts = local_time.slice(0, 10).split('-').map(Number);
    if (parts.length !== 3 || parts.some((n) => Number.isNaN(n))) return null;
    const sun0 = new Date(parts[0], parts[1] - 1, parts[2]).getDay();
    return sun0 === 0 ? 6 : sun0 - 1;
}

export function slotDayBadges(repeat_days, todayIndex = null) {
    const set = new Set(repeat_days || []);
    return DAYS.map((label, i) => ({
        letter: label[0],
        label,
        on: set.has(i),
        today: todayIndex !== null && i === todayIndex,
    }));
}

export function dayDotClass(d) {
    let cls = 'day-dot';
    if (d.on) cls += ' on';
    if (d.today) cls += ' today';
    return cls;
}

export function slotCardMuted(slot) {
    return !slot?.enabled || slot?.state === 'to_be_skipped';
}

export function formatSlotState(state) {
    return state ? String(state).replace(/_/g, ' ') : '—';
}

export function slotStateTone(state) {
    return SLOT_TONE[state] || 'muted';
}

export function slotMinutesFromClock(time) {
    const parts = String(time).split(':');
    if (parts.length !== 2) return null;
    const h = Number(parts[0]);
    const m = Number(parts[1]);
    return Number.isNaN(h) || Number.isNaN(m) ? null : h * 60 + m;
}

export function slotIsPastToday(slot, local_time) {
    if (!slot?.today || !local_time) return false;
    const nowMin = slotMinutesFromClock(local_time.slice(11, 16));
    const slotMin = slotMinutesFromClock(slot.time);
    if (nowMin === null || slotMin === null) return false;
    return slotMin < nowMin;
}

export function slotSkipControl(slot, local_time) {
    if (slot?.enabled === false || !slot?.today || slotIsPastToday(slot, local_time) || SKIP_HIDE.has(slot.state)) return null;
    const active = slot.state === 'to_be_skipped';
    return {
        skip: !active,
        active,
        label: active ? 'Unskip today' : 'Skip today',
    };
}

export function parseApiResponse(text) {
    try {
        return JSON.parse(text);
    } catch {
        return text;
    }
}

export function apiContentType(body) {
    return body === undefined ? undefined : body.startsWith('{') ? 'application/json' : 'text/plain';
}

export function mutationMessage(r, okText, failPrefix) {
    if (r?.ok) return { text: okText, ok: true };
    const err = r?.error || 'unknown';
    return { text: failPrefix ? `${failPrefix}: ${err}` : err, ok: false };
}

export const TIMEZONES = [
    { label: 'UTC', rule: 'UTC0' },
    { label: 'Paris / Brussels / Berlin', rule: 'CET-1CEST,M3.5.0,M10.5.0/3' },
    { label: 'London / Lisbon', rule: 'GMT0BST,M3.5.0/1,M10.5.0' },
    { label: 'Athens / Helsinki', rule: 'EET-2EEST,M3.5.0/3,M10.5.0/4' },
    { label: 'Montreal / New York', rule: 'EST5EDT,M3.2.0,M11.1.0' },
    { label: 'Chicago', rule: 'CST6CDT,M3.2.0,M11.1.0' },
    { label: 'Denver', rule: 'MST7MDT,M3.2.0,M11.1.0' },
    { label: 'Los Angeles', rule: 'PST8PDT,M3.2.0,M11.1.0' },
    { label: 'Réunion', rule: '<+04>-4' },
    { label: 'Guadeloupe / Martinique', rule: 'AST4' },
    { label: 'Tokyo', rule: 'JST-9' },
];

export function timezoneLabel(rule) {
    const tz = TIMEZONES.find((t) => t.rule === rule);
    return tz ? tz.label : 'Custom';
}

export function formatProgress(p) {
    if (!p || p.state === 'idle' || !p.state) return null;
    if (p.state === 'weighing') return 'Weighing…';
    return `Dispensing ${p.portions}/${p.of} · ${p.g} g`;
}

export function formatRssi(dbm) {
    if (dbm === undefined || dbm === null || Number.isNaN(Number(dbm))) return '—';
    const v = Number(dbm);
    const q = v >= -60 ? 'good' : v >= -70 ? 'fair' : 'weak';
    return `${v} dBm · ${q}`;
}

export function withKey(body, password) {
    return password ? `key=${password}\n${body}` : body;
}

export function buildNetworkBody(fields) {
    const body = {};
    for (const k of ['ssid', 'wifi_pass', 'mqtt_host', 'mqtt_user', 'mqtt_pass', 'ntp']) {
        const v = fields[k];
        if (v !== undefined && v !== null && String(v) !== '') body[k] = String(v);
    }
    if (fields.mqtt_port !== undefined && String(fields.mqtt_port) !== '') {
        body.mqtt_port = Number(fields.mqtt_port);
    }
    return body;
}

export function buildDisplayBody({ mode, brightness, battery_off_s, mains_off_s }) {
    return {
        mode,
        brightness: Number(brightness),
        battery_off_s: Number(battery_off_s),
        mains_off_s: Number(mains_off_s),
    };
}

const HIST_SRC = { s: 'Schedule', m: 'App', b: 'Button' };
const HIST_WHY = {
    antijam: 'jam not cleared after 3 tries',
    session: 'took too long',
    io: 'rotation sensor unreadable',
    driver: 'motor did not start/stop',
    led: 'rotation sensor LED',
};

/* One feed-history record (health-monitoring.md § Feed history) for display. */
export function formatHistoryRow(r) {
    const when = r.t ? new Date(r.t * 1000).toLocaleString([], { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' }) : '—';
    const tone = r.res === 'ok' ? 'ok' : r.res === 'underfill' || r.res === 'limit' ? 'warn' : 'bad';
    const notes = [];
    if (r.why && r.why !== '-') notes.push(HIST_WHY[r.why] || r.why);
    if (r.jam && r.jam !== '-') notes.push(`last jam: ${r.jam}`);
    if (r.aj) notes.push(`${r.aj} anti-jam`);
    if (r.busy) notes.push(`${r.busy} sensor reads refused`);
    if (r.err) notes.push(`${r.err} sensor errors`);
    if (!r.bowl) notes.push('bowl missing');
    if (!r.mqtt) notes.push('offline');
    return {
        when,
        source: HIST_SRC[r.src] || r.src,
        want: r.want ? `${r.want} g` : `${r.por} portion(s)`,
        got: `${r.got} g${r.meas ? '' : ' (est.)'}`,
        result: String(r.res || '').replace(/_/g, ' '),
        detail: [`${r.pul}/${r.por} holes`, `${r.dur} s`, `${r.ma} mA max`].concat(notes).join(' · '),
        tone,
    };
}

export function formatUptime(s) {
    const n = Number(s) || 0;
    const d = Math.floor(n / 86400);
    const h = Math.floor((n % 86400) / 3600);
    const m = Math.floor((n % 3600) / 60);
    return d ? `${d} d ${h} h` : h ? `${h} h ${m} min` : `${m} min`;
}

export function buildFeedLimitBody({ daily_max_g, not_eaten_h }) {
    return { daily_max_g: Number(daily_max_g) || 0, not_eaten_h: Number(not_eaten_h) || 0 };
}

/* Page language (web-ui-client.md § Language). English is the source text;
 * French comes from this table, then from the patterns for built strings. */
export function pickLang(statusLang, browserLang) {
    if (statusLang === 'fr' || statusLang === 'en') return statusLang;
    return /^fr/i.test(browserLang || '') ? 'fr' : 'en';
}

export const FR = {
    'Pet Feeder': 'Distributeur', Home: 'Accueil', Schedule: 'Planning', Settings: 'Réglages', System: 'Système', Help: 'Aide',
    'Feeder time': 'Heure du distributeur', Refresh: 'Actualiser', Locked: 'Verrouillé',
    'This feeder has a web password.': 'Ce distributeur est protégé par un mot de passe.',
    'Web password': 'Mot de passe web', Unlock: 'Déverrouiller', Status: 'État', 'Eaten today': 'Mangé aujourd’hui',
    Reset: 'Remettre à 0', 'Feed now': 'Nourrir maintenant', Dispense: 'Distribuer', Grams: 'Grammes',
    Today: 'Aujourd’hui', '+ Add feed': '+ Ajouter un repas', 'Feed next meal': 'Servir le prochain repas',
    'No feeding times yet.': 'Aucun repas programmé.', Feeding: 'Distribution', 'Feed mode': 'Mode',
    'Open loop': 'Simple (sans pesée)', Compensated: 'Compensé (pesée)',
    'Dispense button amount (HA)': 'Quantité du bouton Dispense (HA)',
    'Amount given by the Dispense button in Home Assistant. Empty = one portion.': 'Quantité donnée par le bouton Dispense de Home Assistant. Vide = une portion.',
    'Daily limit (g, 0 = off)': 'Plafond par jour (g, 0 = aucun)', 'Not-eaten alert (h, 0 = off)': 'Alerte « n’a pas mangé » (h, 0 = off)',
    'Child lock': 'Verrou enfant', Save: 'Enregistrer', 'Overfill protection': 'Protection gamelle pleine',
    'Skips scheduled feeds while the bowl already holds this much.': 'Saute les repas planifiés tant que la gamelle contient au moins ce poids.',
    Enabled: 'Activée', 'Full bowl (g)': 'Gamelle pleine (g)', Portion: 'Portion', 'Portion size': 'Taille de portion',
    'Calibrate turns 3 portions (~25 g) into the bowl and weighs them. An empty bowl is not required.': 'Calibrer distribue 3 portions (~25 g) et les pèse. Pas besoin de vider la gamelle. Sinon, la portion s’ajuste toute seule à chaque repas pesé.',
    'Reset to 10 g': 'Revenir à 10 g', Calibrate: 'Calibrer', Screen: 'Écran', Shows: 'Affiche',
    'Bowl weight': 'Poids gamelle', Brightness: 'Luminosité', Off: 'Éteint',
    'Off after, on battery (s)': 'Extinction sur batterie (s)', 'Off after, on mains (s)': 'Extinction sur secteur (s)',
    '0 = always on. Any button press wakes the screen.': '0 = toujours allumé. Un appui sur un bouton rallume l’écran.',
    Scale: 'Balance', Calibration: 'Calibration',
    'Remove the bowl and press Zero; put the empty 350 g bowl back and press Span.': 'Retire la gamelle et appuie sur Zéro ; remets la gamelle vide (350 g) et appuie sur Étalon.',
    'Zero (no bowl)': 'Zéro (sans gamelle)', 'Span (empty bowl)': 'Étalon (gamelle vide)', Battery: 'Batterie',
    'Wi-Fi on battery': 'Wi-Fi sur batterie', 'Always on': 'Toujours', 'Only around feeds': 'Seulement autour des repas',
    Time: 'Heure', Timezone: 'Fuseau horaire', 'POSIX rule': 'Règle POSIX', 'NTP server': 'Serveur NTP',
    Network: 'Réseau', 'Wi-Fi signal': 'Signal Wi-Fi', 'Wi-Fi name (SSID)': 'Nom du Wi-Fi (SSID)', 'Wi-Fi password': 'Mot de passe Wi-Fi',
    'MQTT broker': 'Broker MQTT', 'MQTT port': 'Port MQTT', 'MQTT user': 'Utilisateur MQTT', 'MQTT password': 'Mot de passe MQTT',
    unchanged: 'inchangé',
    'Saving reboots the feeder. If the new Wi-Fi does not connect, it goes back to the previous one.': 'Enregistrer redémarre le distributeur. Si le nouveau Wi-Fi ne se connecte pas, il revient à l’ancien.',
    'Save & reboot': 'Enregistrer et redémarrer', Security: 'Sécurité',
    'New password (4–32 characters, empty = none)': 'Nouveau mot de passe (4 à 32 caractères, vide = aucun)',
    Backup: 'Sauvegarde', 'Settings and schedule. Passwords are never included.': 'Réglages et planning. Les mots de passe ne sont jamais inclus.',
    'Restore…': 'Restaurer…', Download: 'Télécharger', Firmware: 'Firmware', Version: 'Version',
    'Active bank': 'Banque active', Update: 'Mise à jour', 'Firmware folder URL': 'URL du dossier firmware',
    'Install firmware update': 'Installer la mise à jour', Device: 'Appareil', Reboot: 'Redémarrer',
    'Wi-Fi setup again': 'Reconfigurer le Wi-Fi',
    'Factory reset erases Wi-Fi, MQTT, schedule and calibration. Type RESET to confirm.': 'La remise à zéro efface Wi-Fi, MQTT, planning, calibration et historique. Tape RESET pour confirmer.',
    'Factory reset': 'Remise à zéro', 'Feed history': 'Historique des repas', Diagnostics: 'Diagnostic',
    'Show recent log': 'Afficher le journal', Language: 'Langue', Days: 'Jours', Amount: 'Quantité', Cancel: 'Annuler',
    'Feeding time': 'Heure du repas', Clear: 'Effacer',
    'Food in bowl': 'Dans la gamelle', Hopper: 'Réservoir', Activity: 'Activité', Bowl: 'Gamelle', Power: 'Alimentation',
    Normal: 'Normal', Low: 'Bas', Empty: 'Vide', Idle: 'Au repos', 'Dispensing': 'Distribution', OK: 'OK', Mains: 'Secteur',
    Unknown: 'Inconnu', Problem: 'Problème', 'Time not synced': 'Heure non synchronisée', 'Bowl problem': 'Problème de gamelle',
    'Next feed · nothing scheduled': 'Prochain repas · aucun', 'Next feed · now': 'Prochain repas · maintenant',
    'Skip today': 'Sauter aujourd’hui', 'Unskip today': 'Ne plus sauter', 'Weighing…': 'Pesée…',
    Mon: 'Lun', Tue: 'Mar', Wed: 'Mer', Thu: 'Jeu', Fri: 'Ven', Sat: 'Sam', Sun: 'Dim',
    pending: 'à venir', 'to be skipped': 'sera sauté', skipped: 'sauté', 'skipped full': 'sauté (gamelle pleine)',
    'skipped limit': 'sauté (plafond)', dispensing: 'en cours', dispensed: 'servi', failed: 'échec',
    ok: 'ok', underfill: 'incomplet', stuck: 'bloqué', 'empty hopper': 'réservoir vide', aborted: 'annulé', limit: 'plafond',
    App: 'Appli', Button: 'Bouton', offline: 'hors ligne', 'bowl missing': 'gamelle absente',
    'jam not cleared after 3 tries': 'blocage non résolu après 3 essais', 'took too long': 'trop long',
    'rotation sensor unreadable': 'capteur de rotation illisible', 'motor did not start/stop': 'moteur ne démarre/s’arrête pas',
    'rotation sensor LED': 'LED du capteur de rotation', 'No meal yet.': 'Aucun repas pour l’instant.',
    'Meals ok / failed': 'Repas ok / ratés', Jams: 'Blocages', 'MQTT drops': 'Coupures MQTT', Boots: 'Démarrages',
    'Stalled tasks': 'Tâches bloquées', 'Up for': 'Allumé depuis', 'Last reset': 'Dernier redémarrage',
    'Battery voltage': 'Tension batterie', 'Hopper IR beam': 'Faisceau IR réservoir', blocked: 'coupé', clear: 'libre',
    power: 'alimentation', watchdog: 'watchdog', software: 'logiciel',
    Saved: 'Enregistré', 'Not saved': 'Non enregistré', 'Dispense started': 'Distribution lancée', 'Dispense failed': 'Distribution impossible',
    'Feeding time saved': 'Repas enregistré', 'Feeding settings saved': 'Réglages de distribution enregistrés',
    'Limit not saved': 'Plafond non enregistré', 'Child lock saved': 'Verrou enfant enregistré',
    'Overfill protection saved': 'Protection enregistrée', 'Calibrating… (~25 s)': 'Calibration… (~25 s)',
    'Portion size reset': 'Portion remise à 10 g', 'Screen saved': 'Écran enregistré', 'Zero captured': 'Zéro enregistré',
    'Span captured': 'Étalon enregistré', 'Time settings saved': 'Heure enregistrée', 'Saved — rebooting…': 'Enregistré — redémarrage…',
    'Password needs at least 4 characters': 'Le mot de passe doit faire au moins 4 caractères', 'Password set': 'Mot de passe défini',
    'Password removed': 'Mot de passe supprimé', 'Settings restored': 'Réglages restaurés', 'Firmware URL saved': 'URL firmware enregistrée',
    'Rebooting…': 'Redémarrage…', 'Restarting in setup mode…': 'Redémarrage en mode configuration…',
    'Type RESET to confirm': 'Tape RESET pour confirmer', 'Factory reset — restarting in setup mode…': 'Remise à zéro — redémarrage en mode configuration…',
    'Problem cleared': 'Problème effacé', 'Eaten today reset': 'Compteur remis à 0', 'Wrong password': 'Mot de passe incorrect',
    'Next meal served now': 'Prochain repas servi', 'Update started': 'Mise à jour lancée', 'Language saved': 'Langue enregistrée',
    'Bowl removed from the scale?': 'Gamelle retirée de la balance ?', 'Empty 350 g bowl installed?': 'Gamelle vide (350 g) en place ?',
    'Reboot the feeder?': 'Redémarrer le distributeur ?', 'Reset eaten today to 0?': 'Remettre « mangé aujourd’hui » à 0 ?',
    'Reset portion size to 10 g?': 'Remettre la portion à 10 g ?', 'Restore these settings on the feeder?': 'Restaurer ces réglages ?',
    'Save the network settings and reboot the feeder?': 'Enregistrer le réseau et redémarrer ?',
    'Dispense 3 portions (~25 g) into the bowl to measure them?': 'Distribuer 3 portions (~25 g) pour les mesurer ?',
    'Erase the Wi-Fi settings and restart in setup mode (access point)?': 'Effacer le Wi-Fi et redémarrer en mode configuration ?',
    'Serve the next scheduled meal now? It will not be served again at its time.': 'Servir le prochain repas maintenant ? Il ne sera pas resservi à son heure.',
    'Install the firmware from the firmware folder? The feeder reboots when done.': 'Installer le firmware du dossier ? Le distributeur redémarre à la fin.',
    'no meal left today': 'plus de repas prévu aujourd’hui', 'daily limit reached': 'plafond du jour atteint',
    'bowl already full': 'gamelle déjà pleine', 'feeder busy': 'distributeur occupé', 'not accepted': 'refusé',
    'Feed failed': 'Repas raté', 'Hopper empty': 'Réservoir vide', 'Bowl missing at feed': 'Gamelle absente au repas',
    'Connection lost': 'Connexion perdue', 'Daily limit reached': 'Plafond du jour atteint', 'Not eaten': 'N’a pas mangé',
    'Task stalled': 'Tâche bloquée', 'nothing fell': 'rien n’est tombé', 'meal shortened': 'repas réduit', 'limit reached': 'plafond atteint',
    'Not served': 'Non servi', 'Dispense button': 'Bouton Dispense', idle: 'aucune', downloading: 'téléchargement', verifying: 'vérification', error: 'erreur', done: 'terminée',
};

const FR_PATTERNS = [
    [/^Next feed · in (\d+) min$/, 'Prochain repas · dans $1 min'],
    [/^Dispensing (\d+)\/(\d+) · (.+) g$/, 'Distribution $1/$2 · $3 g'],
    [/^Dispensed today: (\d+) g$/, 'Distribué aujourd’hui : $1 g'],
    [/^Delete the (.+) feed\?$/, 'Supprimer le repas de $1 ?'],
    [/^(\d+) portion\(s\)$/, '$1 portion(s)'],
    [/^(\d+)\/(\d+) holes$/, '$1/$2 trous'],
    [/^last jam: (.+)$/, 'dernier blocage : $1'],
    [/^(\d+) anti-jam$/, '$1 anti-blocage'],
    [/^(\d+) sensor reads refused$/, '$1 lectures capteur refusées'],
    [/^(\d+) sensor errors$/, '$1 erreurs capteur'],
    [/^(.+) g \(est\.\)$/, '$1 g (estimé)'],
    [/^(\d+) d (\d+) h$/, '$1 j $2 h'],
    [/^(-?\d+) dBm · good$/, '$1 dBm · bon'],
    [/^(-?\d+) dBm · fair$/, '$1 dBm · moyen'],
    [/^(-?\d+) dBm · weak$/, '$1 dBm · faible'],
    [/^(\d+) min$/, '$1 min'],
    [/^(\d+) h (\d+) min$/, '$1 h $2 min'],
];

export function tr(s, lang) {
    if (lang !== 'fr' || s === undefined || s === null) return s;
    const k = String(s);
    if (Object.prototype.hasOwnProperty.call(FR, k)) return FR[k];
    for (const [re, rep] of FR_PATTERNS) {
        if (re.test(k)) return k.replace(re, rep);
    }
    /* Firmware problem text: "Name (detail); Name" */
    if (k.includes('; ') || /^[A-Z][a-z ]+ \(.+\)$/.test(k)) {
        return k.split('; ').map((part) => {
            const m = part.match(/^(.+?) \((.+)\)$/);
            return m ? `${tr(m[1], lang)} (${tr(m[2], lang)})` : tr(part, lang);
        }).join(' ; ');
    }
    return k;
}

/* Web error codes → words (before translation). */
export function errorWords(code) {
    return {
        no_meal_today: 'no meal left today', limit: 'daily limit reached', bowl_full: 'bowl already full',
        busy: 'feeder busy', rejected: 'not accepted',
    }[code] || code;
}
