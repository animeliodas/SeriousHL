# SeriousHL — движение Half-Life в Serious Sam Classic

Игрок Serious Sam Classic (The First Encounter 1.05, The Second Encounter 1.07) бегает, прыгает, стрейфится в воздухе
и распрыгивается (bunny hop) по правилам Half-Life 1. Всё остальное — Сэма: оружие, монстры, уровни, плавание,
заставки. Плагин [Serious Sam Classics Patch](https://github.com/SamClassicPatch/SuperProject) 1.9.4; порт
[SeriousBridge](https://github.com/animeliodas/SeriousBridge) — ядра для механик других игр в Serious Sam.

> Сделано с помощью Claude Code (ИИ). Проверено скриптом в настоящих играх (TFE и TSE, `tools/test_hl.py`, ниже).
> Только одиночная игра. Не связано с Valve и Croteam.

## Установка (один клик)

1. Скачай `SeriousHL-<версия>.zip` со страницы [Releases](https://github.com/animeliodas/SeriousHL/releases),
   распакуй куда угодно.
2. Запусти `Install.cmd`. Он сам найдёт TFE и TSE в Steam (или GOG; или перетащи папку игры на `Install.cmd`),
   поставит Classics Patch 1.9.4, если его нет (скачает с его страницы релизов на GitHub, проверит SHA-256), положит
   `SeriousHL.dll` в `Bin\Plugins` и ярлык на рабочий стол.
3. Играй с ярлыка (или `Bin\SeriousSam_Custom.exe`; в Steam — параметр запуска `+game ClassicsPatchMod`).

Удаление: `Uninstall.cmd` (Classics Patch остаётся). Без интернета: положи zip Classics Patch
(`TFE1.05_2025_10_15.zip` / `TSE1.07_2025_10_15.zip`) рядом с `Install.cmd`. Вручную: `TFE\SeriousHL.dll` или
`TSE\SeriousHL.dll` из архива — в `<игра>\Bin\Plugins\`.

## Что как в Half-Life

| | Half-Life (`pm_shared.c`) | здесь |
|---|---|---|
| бег | `sv_maxspeed` 320, `sv_accelerate` 10, `sv_friction` 4, `sv_stopspeed` 100 | так же: 320 u/s, остановка с бега за ~0.53 с |
| воздух | `sv_airaccelerate` 10, прибавка вдоль желаемого — до 30 u/s | так же: стрейф мышью + A/D набирает скорость |
| прыжок | 45 юнитов, √(2·800·45) = 268 u/s, до трения | так же: прыжок в тик приземления не теряет скорость — банихоп |
| кап банихопа | в Steam HL: больше 1.7×320 — прыжок режет до 65 % | `hl_bBhopCap 0` (как Bunnymod XT `bxt_bhopcap 0`), `1` — как Steam |
| край | `edgefriction` 2 у обрыва | так же (лучи вниз перед ногами) |
| гравитация | `sv_gravity` 800 | 800, умноженная на гравитацию уровня Сэма к обычной (30 м/с²) |
| урон от падения | с 580 u/s, смерть с 1024 | так же; удары о стены по-сэмовски не ранят |
| присед | ×0.333 к скорости | так же (присед Сэма) |
| кадры | движение считается на каждом кадре (обычно 100 fps) | `hl_iFps 100`: 5 шагов HL в каждом тике Сэма (20 Гц), поворот мыши размазан по ним |

Юнит: глаза Сэма (1.9 м) там же, где у игрока HL (64 юнита) — 1 u = 2.97 см (`hl_fUnit`): бег 9.5 м/с (у Сэма
10), прыжок 1.34 м (у Сэма около 2 м — высокие уступы Сэма бывают не допрыгнуть; `hl_fJumpHeight`).

## Консоль (`~`)

| Команда | По умолчанию | |
|---|---|---|
| `hl_bEnable` | 1 | 0 — движение Сэма |
| `hl_bAutoJump` | 1 | держишь прыжок — прыгаешь на каждом приземлении (как `bxt_autojump`); 0 — прыжок на нажатие, как в HL |
| `hl_bBhopCap` | 0 | 1 — кап Steam Half-Life |
| `hl_bFallDamage` | 1 | урон от падения по HL |
| `hl_iFps` | 100 | частота шагов HL |
| `hl_fMaxSpeed`, `hl_fAccelerate`, `hl_fAirAccelerate`, `hl_fFriction`, `hl_fEdgeFriction`, `hl_fStopSpeed`, `hl_fGravity`, `hl_fJumpHeight`, `hl_fMaxVelocity`, `hl_fForwardSpeed`, `hl_fSideSpeed`, `hl_fUnit` | как в HL | |
| `hl_Status()` | | что сейчас |

В воде, в заставках и автодействиях, мёртвым и с читом полёта движение — Сэма; потом само возвращается к HL.

## Как это сделано

Ядро SeriousBridge (`core/sam/Movement`) через функцию-патч Classics Patch цепляется на
`CMovableEntity::PreMoving` движка: каждый тик, когда Player.es уже применил действие игрока, а движок ещё не сдвинул
его, ускорение и торможение Сэма обнуляются, его собственный прыжок убирается, «ходьба» не останавливает у края;
правила порта получают, что сделал мир (стены, пол, взрывы, движущийся пол), и отдают скорость; гравитация уровня,
которую движок всё равно добавит, вычитается заранее. Именно движение Сэма портило прошлые попытки: на земле он каждый
тик тянет скорость к желаемой (100 м/с², торможение 60), а прыгает только после 0.25 с без вертикального движения —
это и убивало банихоп.

## Сборка и проверка

VS 2022 Build Tools (v143, x86). Рядом должны лежать ядро и SDK Classics Patch:
```
<папка>/serioushl            этот репозиторий
<папка>/seriousbridge        git clone https://github.com/animeliodas/SeriousBridge seriousbridge
<папка>/_deps/SuperProject   git clone --branch 1.9.4 --recurse-submodules https://github.com/SamClassicPatch/SuperProject.git
```
`plugin/build.sh` (Git Bash) → `plugin/dist/Release_TFE105|Release_TSE107/SeriousHL.dll`;
`python tools/make_release.py` → `release/SeriousHL-<версия>.zip`.

Оракул в настоящей игре (`SERIOUSBRIDGE_DEV=1`; `--launch` запускает игру сам, копируя `SaveGame` в сторону):
```
python tools/test_hl.py --game "<папка игры>" --launch
```
каждая проверка — со свежего старта уровня (TFE 11_AlleyOfSphinxes, TSE 2_1_Ziggurrat), клавиши и мышь — тестовыми
хуками плагина, движение пишется в `%TEMP%\SeriousHL\trace.csv`:

| Проверка | TFE | TSE |
|---|---|---|
| бег: 320 u/s по правилам и столько же реально пройдено | PASS 320.0 / 320.1 | PASS 320.0 / 319.9 |
| трение: остановка с 320 u/s за 0.53 с, ~2 м | PASS 0.55 с, 2.39 м | PASS 0.55 с, 2.39 м |
| прыжок: 45 юнитов (1.34 м), ~0.67 с | PASS 1.333 м, 0.70 с | PASS 1.333 м, 0.70 с |
| банихоп: стрейф вправо + поворот 15°/тик, автопрыжок — скорость за 1.75×320 | PASS 622 u/s (18.5 м/с) | PASS 622 u/s |
| кап Steam: прыжок за 1.7× режет скорость | PASS 549 max, 1 срез | PASS 549, 1 срез |
| `hl_bEnable 0`: движение Сэма, без набора; `1` — снова HL | PASS 337 u/s (10 м/с) | PASS 337 u/s |

Если игра встаёт, сторож dev-режима пишет `%TEMP%\SeriousHL\hang.txt` (где главный поток, модуль+смещение), тест
закрывает игру. Грабля: `StartMap` из меню берёт режим заставки (flyover) — TFE 11_AlleyOfSphinxes в нём виснет;
тест ставит `gam_iStartMode=0`, `gam_iStartDifficulty=1`.

## Дальше

Присед в прыжке и даксы (duck-jump, ducktap: в HL присед в воздухе поднимает ноги на 18 юнитов), лестницы и вода по
HL, прыжок по колесу мыши (сейчас — автопрыжок или нажатие), «всё остальное» Half-Life позже.

## Лицензия

GPL-2.0, как Serious Engine 1 и Classics Patch. Правила движения написаны по Half-Life SDK (`pm_shared.c`); кода и
файлов Valve здесь нет.
