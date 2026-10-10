import streamDeck from "@elgato/streamdeck";

import { BatteryAction } from "./actions/battery";
import { HyperXBatteryAction } from "./actions/hyperx-battery";
import { NoiseControlAction } from "./actions/noise-control";
import { SpeakToChatAction } from "./actions/speak-to-chat";

streamDeck.logger.setLevel("info");

streamDeck.actions.registerAction(new BatteryAction());
streamDeck.actions.registerAction(new NoiseControlAction());
streamDeck.actions.registerAction(new SpeakToChatAction());
streamDeck.actions.registerAction(new HyperXBatteryAction());

streamDeck.connect();
