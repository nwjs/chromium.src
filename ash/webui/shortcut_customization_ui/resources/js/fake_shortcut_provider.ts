// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';


/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods: FakeMethodResolver;

  constructor() {
    this.methods = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods.register('getAccelerators');
    this.methods.register('getAcceleratorLayoutInfos');
    this.methods.register('isMutable');
    this.methods.register('addUserAccelerator');
    this.methods.register('replaceAccelerator');
    this.methods.register('removeAccelerator');
    this.methods.register('restoreAllDefaults');
    this.methods.register('restoreActionDefaults');
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.methods.resolveMethod('getAcceleratorLayoutInfos');
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.methods.resolveMethod('getAccelerators');
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    this.methods.setResult(
        'isMutable', {isMutable: source !== AcceleratorSource.kBrowser});
    return this.methods.resolveMethod('isMutable');
  }

  // Return nothing because this method has a void return type.
  addObserver(): void {}

  addUserAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'addUserAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('addUserAccelerator');
  }

  replaceAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'replaceAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('replaceAccelerator');
  }

  removeAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'removeAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('removeAccelerator');
  }

  restoreAllDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'restoreAllDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('restoreAllDefaults');
  }

  restoreActionDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'restoreActionDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('restoreActionDefaults');
  }

  /**
   * Sets the value that will be returned when calling
   * getAccelerators().
   */
  setFakeAcceleratorConfig(config: MojoAcceleratorConfig): void {
    this.methods.setResult('getAccelerators', {config});
  }

  /**
   * Sets the value that will be returned when calling
   * getAcceleratorLayoutInfos().
   */
  setFakeAcceleratorLayoutInfos(layoutInfos: MojoLayoutInfo[]): void {
    this.methods.setResult('getAcceleratorLayoutInfos', {layoutInfos});
  }
}
