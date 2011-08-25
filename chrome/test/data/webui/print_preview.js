// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for print preview WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function PrintPreviewWebUITest() {}

PrintPreviewWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sample page, cause print preview & call our PreLoad().
   **/
  browsePrintPreload: 'print_preview_hello_world_test.html',

  /**
   * Register a mock handler to ensure expectations are met and print preview
   * behaves correctly.
   **/
  PreLoad: function() {

    /**
     * Create a handler class with empty methods to allow mocking to register
     * expectations and for registration of handlers with chrome.send.
     **/
    function MockPrintPreviewHandler() {}

    MockPrintPreviewHandler.prototype = {
      getDefaultPrinter: function() {},
      getPrinters: function() {},
      getPreview: function(settings) {},
      print: function(settings) {},
      getPrinterCapabilities: function(printerName) {},
      showSystemDialog: function() {},
      managePrinters: function() {},
      closePrintPreviewTab: function() {},
      hidePreview: function() {},
    };

    // Create the actual mock and register stubs for methods expected to be
    // called before our tests run.  Specific expectations can be made in the
    // tests themselves.
    var mockHandler = this.mockHandler = mock(MockPrintPreviewHandler);
    mockHandler.stubs().getDefaultPrinter().
        will(callFunction(function() {
          setDefaultPrinter('FooDevice');
        }));
    mockHandler.stubs().getPrinterCapabilities(NOT_NULL).
        will(callFunction(function() {
          updateWithPrinterCapabilities({
            disableColorOption: true,
            setColorAsDefault: true,
            disableCopiesOption: true,
          });
        }));
    mockHandler.stubs().getPreview(NOT_NULL).
        will(callFunction(function() {
          updatePrintPreview(1, 'title', true);
        }));

    mockHandler.stubs().getPrinters().
        will(callFunction(function() {
          setPrinters([{
              printerName: 'FooName',
              deviceName: 'FooDevice',
            }, {
              printerName: 'BarName',
              deviceName: 'BarDevice',
            },
          ]);
        }));

    // Register our mock as a handler of the chrome.send messages.
    registerMockMessageCallbacks(mockHandler, MockPrintPreviewHandler);
  },
  testGenPreamble: function(testFixture, testName) {
    GEN('  if (!HasPDFLib()) {');
    GEN('    LOG(WARNING)');
    GEN('        << "Skipping test ' + testFixture + '.' + testName + '"');
    GEN('        << ": No PDF Lib.";');
    GEN('    SUCCEED();');
    GEN('    return;');
    GEN('  }');
  },
  typedefCppFixture: null,
};

GEN('#include "base/command_line.h"');
GEN('#include "base/path_service.h"');
GEN('#include "base/stringprintf.h"');
GEN('#include "chrome/browser/ui/webui/web_ui_browsertest.h"');
GEN('#include "chrome/common/chrome_paths.h"');
GEN('#include "chrome/common/chrome_switches.h"');
GEN('');
GEN('class PrintPreviewWebUITest');
GEN('    : public WebUIBrowserTest {');
GEN(' protected:');
GEN('  // WebUIBrowserTest override.');
GEN('  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {');
GEN('    WebUIBrowserTest::SetUpCommandLine(command_line);');
GEN('    command_line->AppendSwitch(switches::kEnablePrintPreview);');
GEN('  }');
GEN('');
GEN('  bool HasPDFLib() const {');
GEN('    FilePath pdf;');
GEN('    return PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf) &&');
GEN('        file_util::PathExists(pdf);');
GEN('  }');
GEN('};');
GEN('');

TEST_F('PrintPreviewWebUITest', 'FLAKY_TestPrinterList', function() {
  var printerList = $('printer-list');
  assertTrue(!!printerList, 'printerList');
  assertTrue(printerList.options.length >= 2, 'printer-list has at least 2');
  expectEquals('FooName', printerList.options[0].text, '0 text is FooName');
  expectEquals('FooDevice', printerList.options[0].value,
               '0 value is FooDevice');
  expectEquals('BarName', printerList.options[1].text, '1 text is BarName');
  expectEquals('BarDevice', printerList.options[1].value,
               '1 value is BarDevice');
});

/**
 * Verify that |section| visibility matches |visible|.
 * @param {HTMLDivElement} section The section to check.
 * @param {boolean} visible The expected state of visibility.
 **/
function checkSectionVisible(section, visible) {
  assertNotEquals(null, section);
  expectEquals(section.classList.contains('visible'), visible,
               'section=' + section);
}

// Test that disabled settings hide the disabled sections.
// crbug.com/90476
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestSectionsDisabled', function() {
  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: true,
          setColorAsDefault: true,
          disableCopiesOption: true,
          disableLandscapeOption: true,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();

  checkSectionVisible(layoutSettings.layoutOption_, false);
  checkSectionVisible(colorSettings.colorOption_, false);
  checkSectionVisible(copiesSettings.copiesOption_, false);
});

// Test that the color settings are set according to the printer capabilities.
// crbug.com/90476
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestColorSettings', function() {
  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: false,
          setColorAsDefault: true,
          disableCopiesOption: false,
          disableLandscapeOption: false,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();
  expectTrue(colorSettings.colorRadioButton.checked);
  expectFalse(colorSettings.bwRadioButton.checked);

  this.mockHandler.expects(once()).getPrinterCapabilities('FooDevice').
      will(callFunction(function() {
        updateWithPrinterCapabilities({
          disableColorOption: false,
          setColorAsDefault: false,
          disableCopiesOption: false,
          disableLandscapeOption: false,
        });
      }));

  updateControlsWithSelectedPrinterCapabilities();
  expectFalse(colorSettings.colorRadioButton.checked);
  expectTrue(colorSettings.bwRadioButton.checked);
});

// Test that changing the selected printer updates the preview.
// crbug.com/90476
TEST_F('PrintPreviewWebUITest', 'FLAKY_TestPrinterChangeUpdatesPreview',
       function() {
  var matchAnythingSave = new SaveArgumentsMatcher(ANYTHING);

  this.mockHandler.expects(once()).getPreview(matchAnythingSave).
      will(callFunction(function() {
        updatePrintPreview('title', true, 2,
                           matchAnythingSave.argument.requestID);
      }));

  var printerList = $('printer-list');
  assertNotEquals(null, printerList, 'printerList');
  assertGE(printerList.options.length, printerListMinLength);
  expectEquals(fooIndex, printerList.selectedIndex,
               'fooIndex=' + fooIndex);
  var oldLastPreviewRequestID = lastPreviewRequestID;
  ++printerList.selectedIndex;
  updateControlsWithSelectedPrinterCapabilities();
  expectNotEquals(oldLastPreviewRequestID, lastPreviewRequestID);
});

/**
 * Test fixture to test case when no PDF plugin exists.
 * @extends {PrintPreviewWebUITest}
 * @constructor
 **/
function PrintPreviewNoPDFWebUITest() {}

PrintPreviewNoPDFWebUITest.prototype = {
  __proto__: PrintPreviewWebUITest.prototype,

  /**
   * Provide a typedef for C++ to correspond to JS subclass.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'PrintPreviewWebUITest',

  /**
   * Always return false to simulate failure and check expected error condition.
   * @return {boolean} Always false.
   * @override
   */
  checkCompatiblePluginExists: function() {
    return false;
  },
};

// Test that error message is displayed when plugin doesn't exist.
// crbug.com/90476
TEST_F('PrintPreviewNoPDFWebUITest', 'FLAKY_TestErrorMessage', function() {
  var errorButton = $('error-button');
  assertNotEquals(null, errorButton);
  expectFalse(errorButton.disabled);
  var errorText = $('error-text');
  assertNotEquals(null, errorText);
  expectFalse(errorText.classList.contains('hidden'));
});
