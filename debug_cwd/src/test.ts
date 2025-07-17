import { test } from './test_import';
import { test as nestedTest } from './nested/test';
import { FilePermissions, readFileText } from '__internal:fs';

console.log(`test: ${test}`);
nestedTest();

const blast: FilePermissions = FilePermissions.OwnerRead;
console.log(blast);
console.log(U16_MAX);

async function testAsync() {
    const contents = await readFileText('test.txt');
    console.log(contents);
}

testAsync();
console.log('testAsync called');
