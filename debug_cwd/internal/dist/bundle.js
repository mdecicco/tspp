define("src/test_import", ["require", "exports"], function (require, exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.test = void 0;
    exports.test = 32;
});
define("src/nested/test", ["require", "exports"], function (require, exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.test = test;
    function test() {
        console.log('test');
    }
});
define("src/test", ["require", "exports", "src/test_import", "src/nested/test", "__internal:fs"], function (require, exports, test_import_1, test_1, __internal_fs_1) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    console.log(`test: ${test_import_1.test}`);
    (0, test_1.test)();
    const blast = __internal_fs_1.FilePermissions.OwnerRead;
    console.log(blast);
    console.log(U16_MAX);
    async function testAsync() {
        const contents = await (0, __internal_fs_1.readFileText)('test.txt');
        console.log(contents);
    }
    testAsync();
    console.log('testAsync called');
});
