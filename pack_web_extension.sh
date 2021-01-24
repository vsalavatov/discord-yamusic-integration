#!/bin/bash

cd browser-extension && zip webext.zip *.js manifest.json && cd .. && mv browser-extension/webext.zip webext.xpi