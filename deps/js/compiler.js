()=>{
    const fs = require('__internal:fs');
    const process = require('process');
    const path = require('path');

    function requireMinimatch() {
        //
        // concat-map
        //
        var concatMap = (() => {
            var isArray = Array.isArray || function (xs) {
                return Object.prototype.toString.call(xs) === '[object Array]';
            };
        
            return (xs, fn) => {
                var res = [];
                for (var i = 0; i < xs.length; i++) {
                    var x = fn(xs[i], i);
                    if (isArray(x)) res.push.apply(res, x);
                    else res.push(x);
                }
                return res;
            };
        })();
        
        //
        // balanced-match
        //
        var balanced = (() => {
            function b(a, b, str) {
                if (a instanceof RegExp) a = maybeMatch(a, str);
                if (b instanceof RegExp) b = maybeMatch(b, str);
                
                var r = range(a, b, str);
                
                return r && {
                    start: r[0],
                    end: r[1],
                    pre: str.slice(0, r[0]),
                    body: str.slice(r[0] + a.length, r[1]),
                    post: str.slice(r[1] + b.length)
                };
            }
            
            function maybeMatch(reg, str) {
                var m = str.match(reg);
                return m ? m[0] : null;
            }
            
            b.range = range;
            function range(a, b, str) {
                var begs, beg, left, right, result;
                var ai = str.indexOf(a);
                var bi = str.indexOf(b, ai + 1);
                var i = ai;
                
                if (ai >= 0 && bi > 0) {
                    if(a===b) {
                        return [ai, bi];
                    }
                    begs = [];
                    left = str.length;
                
                    while (i >= 0 && !result) {
                        if (i == ai) {
                            begs.push(i);
                            ai = str.indexOf(a, i + 1);
                        } else if (begs.length == 1) {
                            result = [ begs.pop(), bi ];
                        } else {
                            beg = begs.pop();
                            if (beg < left) {
                            left = beg;
                            right = bi;
                            }
                    
                            bi = str.indexOf(b, i + 1);
                        }
                    
                        i = ai < bi && ai >= 0 ? ai : bi;
                    }
                
                    if (begs.length) {
                        result = [ left, right ];
                    }
                }
                
                return result;
            }
        
            return b;
        })();
        
        //
        // brace-expansion
        //
        
        var expand = (() => {
            var escSlash = '\0SLASH'+Math.random()+'\0';
            var escOpen = '\0OPEN'+Math.random()+'\0';
            var escClose = '\0CLOSE'+Math.random()+'\0';
            var escComma = '\0COMMA'+Math.random()+'\0';
            var escPeriod = '\0PERIOD'+Math.random()+'\0';
        
            function numeric(str) {
                return parseInt(str, 10) == str
                    ? parseInt(str, 10)
                    : str.charCodeAt(0);
            }
            function escapeBraces(str) {
                return str.split('\\\\').join(escSlash)
                            .split('\\{').join(escOpen)
                            .split('\\}').join(escClose)
                            .split('\\,').join(escComma)
                            .split('\\.').join(escPeriod);
            }
            function unescapeBraces(str) {
                return str.split(escSlash).join('\\')
                            .split(escOpen).join('{')
                            .split(escClose).join('}')
                            .split(escComma).join(',')
                            .split(escPeriod).join('.');
            }
            function parseCommaParts(str) {
                if (!str)
                    return [''];
            
                var parts = [];
                var m = balanced('{', '}', str);
            
                if (!m)
                    return str.split(',');
            
                var pre = m.pre;
                var body = m.body;
                var post = m.post;
                var p = pre.split(',');
            
                p[p.length-1] += '{' + body + '}';
                var postParts = parseCommaParts(post);
                if (post.length) {
                    p[p.length-1] += postParts.shift();
                    p.push.apply(p, postParts);
                }
            
                parts.push.apply(parts, p);
            
                return parts;
            }
            function expandTop(str) {
                if (!str)
                    return [];
            
                // I don't know why Bash 4.3 does this, but it does.
                // Anything starting with {} will have the first two bytes preserved
                // but *only* at the top level, so {},a}b will not expand to anything,
                // but a{},b}c will be expanded to [a}c,abc].
                // One could argue that this is a bug in Bash, but since the goal of
                // this module is to match Bash's rules, we escape a leading {}
                if (str.substr(0, 2) === '{}') {
                    str = '\\{\\}' + str.substr(2);
                }
            
                return expand(escapeBraces(str), true).map(unescapeBraces);
            }
            function identity(e) {
                return e;
            }
            function embrace(str) {
                return '{' + str + '}';
            }
            function isPadded(el) {
                return /^-?0\d/.test(el);
            }
            function lte(i, y) {
                return i <= y;
            }
            function gte(i, y) {
                return i >= y;
            }
            function expand(str, isTop) {
                var expansions = [];
            
                var m = balanced('{', '}', str);
                if (!m || /\$$/.test(m.pre)) return [str];
            
                var isNumericSequence = /^-?\d+\.\.-?\d+(?:\.\.-?\d+)?$/.test(m.body);
                var isAlphaSequence = /^[a-zA-Z]\.\.[a-zA-Z](?:\.\.-?\d+)?$/.test(m.body);
                var isSequence = isNumericSequence || isAlphaSequence;
                var isOptions = m.body.indexOf(',') >= 0;
                if (!isSequence && !isOptions) {
                    // {a},b}
                    if (m.post.match(/,.*\}/)) {
                        str = m.pre + '{' + m.body + escClose + m.post;
                        return expand(str);
                    }
                    return [str];
                }
            
                var n;
                if (isSequence) {
                    n = m.body.split(/\.\./);
                } else {
                    n = parseCommaParts(m.body);
                    if (n.length === 1) {
                        // x{{a,b}}y ==> x{a}y x{b}y
                        n = expand(n[0], false).map(embrace);
                        if (n.length === 1) {
                            var post = m.post.length
                            ? expand(m.post, false)
                            : [''];
                            return post.map(function(p) {
                            return m.pre + n[0] + p;
                            });
                        }
                    }
                }
            
                // at this point, n is the parts, and we know it's not a comma set
                // with a single entry.
            
                // no need to expand pre, since it is guaranteed to be free of brace-sets
                var pre = m.pre;
                var post = m.post.length
                    ? expand(m.post, false)
                    : [''];
            
                var N;
            
                if (isSequence) {
                    var x = numeric(n[0]);
                    var y = numeric(n[1]);
                    var width = Math.max(n[0].length, n[1].length)
                    var incr = n.length == 3 ? Math.abs(numeric(n[2])) : 1;
                    var test = lte;
                    var reverse = y < x;
                    if (reverse) {
                        incr *= -1;
                        test = gte;
                    }
                    var pad = n.some(isPadded);
            
                    N = [];
            
                    for (var i = x; test(i, y); i += incr) {
                        var c;
                        if (isAlphaSequence) {
                            c = String.fromCharCode(i);
                            if (c === '\\')
                            c = '';
                        } else {
                            c = String(i);
                            if (pad) {
                            var need = width - c.length;
                            if (need > 0) {
                                var z = new Array(need + 1).join('0');
                                if (i < 0)
                                c = '-' + z + c.slice(1);
                                else
                                c = z + c;
                            }
                            }
                        }
                        N.push(c);
                    }
                } else {
                    N = concatMap(n, function(el) { return expand(el, false) });
                }
            
                for (var j = 0; j < N.length; j++) {
                    for (var k = 0; k < post.length; k++) {
                        var expansion = pre + N[j] + post[k];
                        if (!isTop || isSequence || expansion)
                            expansions.push(expansion);
                    }
                }
            
                return expansions;
            }
        
            return expandTop;
        })();
        
        //
        // Minimatch
        //
        minimatch.Minimatch = Minimatch
        
        var path = {
            sep: '/'
        }
        minimatch.sep = path.sep
        
        var GLOBSTAR = minimatch.GLOBSTAR = Minimatch.GLOBSTAR = {}
        
        var plTypes = {
            '!': { open: '(?:(?!(?:', close: '))[^/]*?)'},
            '?': { open: '(?:', close: ')?' },
            '+': { open: '(?:', close: ')+' },
            '*': { open: '(?:', close: ')*' },
            '@': { open: '(?:', close: ')' }
        }
        
        // any single thing other than /
        // don't need to escape / when using new RegExp()
        var qmark = '[^/]'
        
        // * => any number of characters
        var star = qmark + '*?'
        
        // ** when dots are allowed.  Anything goes, except .. and .
        // not (^ or / followed by one or two dots followed by $ or /),
        // followed by anything, any number of times.
        var twoStarDot = '(?:(?!(?:\\\/|^)(?:\\.{1,2})($|\\\/)).)*?'
        
        // not a ^ or / followed by a dot,
        // followed by anything, any number of times.
        var twoStarNoDot = '(?:(?!(?:\\\/|^)\\.).)*?'
        
        // characters that need to be escaped in RegExp.
        var reSpecials = charSet('().*{}+?[]^$\\!')
        
        // "abc" -> { a:true, b:true, c:true }
        function charSet (s) {
            return s.split('').reduce(function (set, c) {
                set[c] = true
                return set
            }, {})
        }
        
        // normalizes slashes.
        var slashSplit = /\/+/
        
        minimatch.filter = filter
        function filter (pattern, options) {
            options = options || {}
            return function (p, i, list) {
                return minimatch(p, pattern, options)
            }
        }
        function ext (a, b) {
            b = b || {}
            var t = {}
            Object.keys(a).forEach(function (k) { t[k] = a[k] })
            Object.keys(b).forEach(function (k) { t[k] = b[k] })
            return t
        }
        minimatch.defaults = function (def) {
            if (!def || typeof def !== 'object' || !Object.keys(def).length) {
                return minimatch
            }
        
            var orig = minimatch
        
            var m = function minimatch (p, pattern, options) {
                return orig(p, pattern, ext(def, options))
            }
            m.Minimatch = function Minimatch (pattern, options) {
                return new orig.Minimatch(pattern, ext(def, options))
            }
            m.Minimatch.defaults = function defaults (options) {
                return orig.defaults(ext(def, options)).Minimatch
            }
            m.filter = function filter (pattern, options) {
                return orig.filter(pattern, ext(def, options))
            }
            m.defaults = function defaults (options) {
                return orig.defaults(ext(def, options))
            }
            m.makeRe = function makeRe (pattern, options) {
                return orig.makeRe(pattern, ext(def, options))
            }
            m.braceExpand = function braceExpand (pattern, options) {
                return orig.braceExpand(pattern, ext(def, options))
            }
            m.match = function (list, pattern, options) {
                return orig.match(list, pattern, ext(def, options))
            }
        
            return m
        }
        Minimatch.defaults = function (def) {
            return minimatch.defaults(def).Minimatch
        }
        function minimatch (p, pattern, options) {
            assertValidPattern(pattern)
        
            if (!options) options = {}
        
            // shortcut: comments match nothing.
            if (!options.nocomment && pattern.charAt(0) === '#') {
                return false
            }
        
            return new Minimatch(pattern, options).match(p)
        }
        function Minimatch (pattern, options) {
            if (!(this instanceof Minimatch)) {
                return new Minimatch(pattern, options)
            }
        
            assertValidPattern(pattern)
        
            if (!options) options = {}
        
            pattern = pattern.trim()
        
            // windows support: need to use /, not \
            if (!options.allowWindowsEscape && path.sep !== '/') {
                pattern = pattern.split(path.sep).join('/')
            }
        
            this.options = options
            this.set = []
            this.pattern = pattern
            this.regexp = null
            this.negate = false
            this.comment = false
            this.empty = false
            this.partial = !!options.partial
        
            // make the set of regexps etc.
            this.make()
        }
        
        Minimatch.prototype.debug = function () {}
        
        Minimatch.prototype.make = make
        function make () {
            var pattern = this.pattern
            var options = this.options
        
            // empty patterns and comments match nothing.
            if (!options.nocomment && pattern.charAt(0) === '#') {
                this.comment = true
                return
            }
            if (!pattern) {
                this.empty = true
                return
            }
        
            // step 1: figure out negation, etc.
            this.parseNegate()
        
            // step 2: expand braces
            var set = this.globSet = this.braceExpand()
        
            if (options.debug) this.debug = function debug() { console.error.apply(console, arguments) }
        
            this.debug(this.pattern, set)
        
            // step 3: now we have a set, so turn each one into a series of path-portion
            // matching patterns.
            // These will be regexps, except in the case of "**", which is
            // set to the GLOBSTAR object for globstar behavior,
            // and will not contain any / characters
            set = this.globParts = set.map(function (s) {
                return s.split(slashSplit)
            })
        
            this.debug(this.pattern, set)
        
            // glob --> regexps
            set = set.map(function (s, si, set) {
                return s.map(this.parse, this)
            }, this)
        
            this.debug(this.pattern, set)
        
            // filter out everything that didn't compile properly.
            set = set.filter(function (s) {
                return s.indexOf(false) === -1
            })
        
            this.debug(this.pattern, set)
        
            this.set = set
        }
        
        Minimatch.prototype.parseNegate = parseNegate
        function parseNegate () {
            var pattern = this.pattern
            var negate = false
            var options = this.options
            var negateOffset = 0
        
            if (options.nonegate) return
        
            for (var i = 0, l = pattern.length; i < l && pattern.charAt(i) === '!'; i++) {
                negate = !negate
                negateOffset++
            }
        
            if (negateOffset) this.pattern = pattern.substr(negateOffset)
            this.negate = negate
        }
        
        // Brace expansion:
        // a{b,c}d -> abd acd
        // a{b,}c -> abc ac
        // a{0..3}d -> a0d a1d a2d a3d
        // a{b,c{d,e}f}g -> abg acdfg acefg
        // a{b,c}d{e,f}g -> abdeg acdeg abdeg abdfg
        //
        // Invalid sets are not expanded.
        // a{2..}b -> a{2..}b
        // a{b}c -> a{b}c
        minimatch.braceExpand = function (pattern, options) {
            return braceExpand(pattern, options)
        }
        
        Minimatch.prototype.braceExpand = braceExpand
        
        function braceExpand (pattern, options) {
            if (!options) {
                if (this instanceof Minimatch) {
                    options = this.options
                } else {
                    options = {}
                }
            }
        
            pattern = typeof pattern === 'undefined' ? this.pattern : pattern
        
            assertValidPattern(pattern)
        
            // Thanks to Yeting Li <https://github.com/yetingli> for
            // improving this regexp to avoid a ReDOS vulnerability.
            if (options.nobrace || !/\{(?:(?!\{).)*\}/.test(pattern)) {
                // shortcut. no need to expand.
                return [pattern]
            }
        
            return expand(pattern)
        }
        
        var MAX_PATTERN_LENGTH = 1024 * 64
        var assertValidPattern = function (pattern) {
            if (typeof pattern !== 'string') {
                throw new TypeError('invalid pattern')
            }
        
            if (pattern.length > MAX_PATTERN_LENGTH) {
                throw new TypeError('pattern is too long')
            }
        }
        
        // parse a component of the expanded set.
        // At this point, no pattern may contain "/" in it
        // so we're going to return a 2d array, where each entry is the full
        // pattern, split on '/', and then turned into a regular expression.
        // A regexp is made at the end which joins each array with an
        // escaped /, and another full one which joins each regexp with |.
        //
        // Following the lead of Bash 4.1, note that "**" only has special meaning
        // when it is the *only* thing in a path portion.  Otherwise, any series
        // of * is equivalent to a single *.  Globstar behavior is enabled by
        // default, and can be disabled by setting options.noglobstar.
        Minimatch.prototype.parse = parse
        var SUBPARSE = {}
        function parse (pattern, isSub) {
            assertValidPattern(pattern)
        
            var options = this.options
        
            // shortcuts
            if (pattern === '**') {
                if (!options.noglobstar)
                    return GLOBSTAR
                else
                    pattern = '*'
            }
            if (pattern === '') return ''
        
            var re = ''
            var hasMagic = !!options.nocase
            var escaping = false
            // ? => one single character
            var patternListStack = []
            var negativeLists = []
            var stateChar
            var inClass = false
            var reClassStart = -1
            var classStart = -1
            // . and .. never match anything that doesn't start with .,
            // even when options.dot is set.
            var patternStart = pattern.charAt(0) === '.' ? '' // anything
            // not (start or / followed by . or .. followed by / or end)
            : options.dot ? '(?!(?:^|\\\/)\\.{1,2}(?:$|\\\/))' : '(?!\\.)'
            var self = this
        
            function clearStateChar () {
                if (stateChar) {
                    // we had some state-tracking character
                    // that wasn't consumed by this pass.
                    switch (stateChar) {
                        case '*':
                            re += star
                            hasMagic = true
                        break
                        case '?':
                            re += qmark
                            hasMagic = true
                        break
                        default:
                            re += '\\' + stateChar
                        break
                    }
                    self.debug('clearStateChar %j %j', stateChar, re)
                    stateChar = false
                }
            }
        
            for (var i = 0, len = pattern.length, c; (i < len) && (c = pattern.charAt(i)); i++) {
                this.debug('%s\t%s %s %j', pattern, i, re, c)
            
                // skip over any that are escaped.
                if (escaping && reSpecials[c]) {
                    re += '\\' + c
                    escaping = false
                    continue
                }
            
                switch (c) {
                    /* istanbul ignore next */
                    case '/': {
                        // completely not allowed, even escaped.
                        // Should already be path-split by now.
                        return false
                    }
            
                    case '\\':
                        clearStateChar()
                        escaping = true
                        continue
                
                        // the various stateChar values
                        // for the "extglob" stuff.
                    case '?':
                    case '*':
                    case '+':
                    case '@':
                    case '!':
                        this.debug('%s\t%s %s %j <-- stateChar', pattern, i, re, c)
                
                        // all of those are literals inside a class, except that
                        // the glob [!a] means [^a] in regexp
                        if (inClass) {
                            this.debug('  in class')
                            if (c === '!' && i === classStart + 1) c = '^'
                            re += c
                            continue
                        }
                
                        // if we already have a stateChar, then it means
                        // that there was something like ** or +? in there.
                        // Handle the stateChar, then proceed with this one.
                        self.debug('call clearStateChar %j', stateChar)
                        clearStateChar()
                        stateChar = c
                        // if extglob is disabled, then +(asdf|foo) isn't a thing.
                        // just clear the statechar *now*, rather than even diving into
                        // the patternList stuff.
                        if (options.noext) clearStateChar()
                        continue
                    case '(':
                        if (inClass) {
                            re += '('
                            continue
                        }
                
                        if (!stateChar) {
                            re += '\\('
                            continue
                        }
                
                        patternListStack.push({
                            type: stateChar,
                            start: i - 1,
                            reStart: re.length,
                            open: plTypes[stateChar].open,
                            close: plTypes[stateChar].close
                        })
                        // negation is (?:(?!js)[^/]*)
                        re += stateChar === '!' ? '(?:(?!(?:' : '(?:'
                        this.debug('plType %j %j', stateChar, re)
                        stateChar = false
                        continue
                    case ')':
                        if (inClass || !patternListStack.length) {
                            re += '\\)'
                            continue
                        }
                
                        clearStateChar()
                        hasMagic = true
                        var pl = patternListStack.pop()
                        // negation is (?:(?!js)[^/]*)
                        // The others are (?:<pattern>)<type>
                        re += pl.close
                        if (pl.type === '!') {
                            negativeLists.push(pl)
                        }
                        pl.reEnd = re.length
                        continue
                    case '|':
                        if (inClass || !patternListStack.length || escaping) {
                            re += '\\|'
                            escaping = false
                            continue
                        }
                
                        clearStateChar()
                        re += '|'
                        continue
                    // these are mostly the same in regexp and glob
                    case '[':
                        // swallow any state-tracking char before the [
                        clearStateChar()
                
                        if (inClass) {
                            re += '\\' + c
                            continue
                        }
                
                        inClass = true
                        classStart = i
                        reClassStart = re.length
                        re += c
                        continue
                    case ']':
                        //  a right bracket shall lose its special
                        //  meaning and represent itself in
                        //  a bracket expression if it occurs
                        //  first in the list.  -- POSIX.2 2.8.3.2
                        if (i === classStart + 1 || !inClass) {
                            re += '\\' + c
                            escaping = false
                            continue
                        }
                
                        // handle the case where we left a class open.
                        // "[z-a]" is valid, equivalent to "\[z-a\]"
                        // split where the last [ was, make sure we don't have
                        // an invalid re. if so, re-walk the contents of the
                        // would-be class to re-translate any characters that
                        // were passed through as-is
                        // TODO: It would probably be faster to determine this
                        // without a try/catch and a new RegExp, but it's tricky
                        // to do safely.  For now, this is safe and works.
                        var cs = pattern.substring(classStart + 1, i)
                        try {
                            RegExp('[' + cs + ']')
                        } catch (er) {
                            // not a valid class!
                            var sp = this.parse(cs, SUBPARSE)
                            re = re.substr(0, reClassStart) + '\\[' + sp[0] + '\\]'
                            hasMagic = hasMagic || sp[1]
                            inClass = false
                            continue
                        }
                
                        // finish up the class.
                        hasMagic = true
                        inClass = false
                        re += c
                        continue
                    default:
                        // swallow any state char that wasn't consumed
                        clearStateChar()
                
                        if (escaping) {
                            // no need
                            escaping = false
                        } else if (reSpecials[c]
                            && !(c === '^' && inClass)) {
                            re += '\\'
                        }
                
                        re += c
                } // switch
            } // for
        
            // handle the case where we left a class open.
            // "[abc" is valid, equivalent to "\[abc"
            if (inClass) {
                // split where the last [ was, and escape it
                // this is a huge pita.  We now have to re-walk
                // the contents of the would-be class to re-translate
                // any characters that were passed through as-is
                cs = pattern.substr(classStart + 1)
                sp = this.parse(cs, SUBPARSE)
                re = re.substr(0, reClassStart) + '\\[' + sp[0]
                hasMagic = hasMagic || sp[1]
            }
        
            // handle the case where we had a +( thing at the *end*
            // of the pattern.
            // each pattern list stack adds 3 chars, and we need to go through
            // and escape any | chars that were passed through as-is for the regexp.
            // Go through and escape them, taking care not to double-escape any
            // | chars that were already escaped.
            for (pl = patternListStack.pop(); pl; pl = patternListStack.pop()) {
                var tail = re.slice(pl.reStart + pl.open.length)
                this.debug('setting tail', re, pl)
                // maybe some even number of \, then maybe 1 \, followed by a |
                tail = tail.replace(/((?:\\{2}){0,64})(\\?)\|/g, function (_, $1, $2) {
                    if (!$2) {
                    // the | isn't already escaped, so escape it.
                    $2 = '\\'
                    }
            
                    // need to escape all those slashes *again*, without escaping the
                    // one that we need for escaping the | character.  As it works out,
                    // escaping an even number of slashes can be done by simply repeating
                    // it exactly after itself.  That's why this trick works.
                    //
                    // I am sorry that you have to see this.
                    return $1 + $1 + $2 + '|'
                })
            
                this.debug('tail=%j\n   %s', tail, tail, pl, re)
                var t = pl.type === '*' ? star
                    : pl.type === '?' ? qmark
                    : '\\' + pl.type
            
                hasMagic = true
                re = re.slice(0, pl.reStart) + t + '\\(' + tail
            }
        
            // handle trailing things that only matter at the very end.
            clearStateChar()
            if (escaping) {
                // trailing \\
                re += '\\\\'
            }
        
            // only need to apply the nodot start if the re starts with
            // something that could conceivably capture a dot
            var addPatternStart = false
            switch (re.charAt(0)) {
                case '[':
                case '.':
                case '(': addPatternStart = true
            }
        
            // Hack to work around lack of negative lookbehind in JS
            // A pattern like: *.!(x).!(y|z) needs to ensure that a name
            // like 'a.xyz.yz' doesn't match.  So, the first negative
            // lookahead, has to look ALL the way ahead, to the end of
            // the pattern.
            for (var n = negativeLists.length - 1; n > -1; n--) {
                var nl = negativeLists[n]
            
                var nlBefore = re.slice(0, nl.reStart)
                var nlFirst = re.slice(nl.reStart, nl.reEnd - 8)
                var nlLast = re.slice(nl.reEnd - 8, nl.reEnd)
                var nlAfter = re.slice(nl.reEnd)
            
                nlLast += nlAfter
            
                // Handle nested stuff like *(*.js|!(*.json)), where open parens
                // mean that we should *not* include the ) in the bit that is considered
                // "after" the negated section.
                var openParensBefore = nlBefore.split('(').length - 1
                var cleanAfter = nlAfter
                for (i = 0; i < openParensBefore; i++) {
                    cleanAfter = cleanAfter.replace(/\)[+*?]?/, '')
                }
                nlAfter = cleanAfter
            
                var dollar = ''
                if (nlAfter === '' && isSub !== SUBPARSE) {
                    dollar = '$'
                }
                var newRe = nlBefore + nlFirst + nlAfter + dollar + nlLast
                re = newRe
            }
        
            // if the re is not "" at this point, then we need to make sure
            // it doesn't match against an empty path part.
            // Otherwise a/* will match a/, which it should not.
            if (re !== '' && hasMagic) {
                re = '(?=.)' + re
            }
        
            if (addPatternStart) {
                re = patternStart + re
            }
        
            // parsing just a piece of a larger pattern.
            if (isSub === SUBPARSE) {
                return [re, hasMagic]
            }
        
            // skip the regexp for non-magical patterns
            // unescape anything in it, though, so that it'll be
            // an exact match against a file etc.
            if (!hasMagic) {
                return globUnescape(pattern)
            }
        
            var flags = options.nocase ? 'i' : ''
            try {
                var regExp = new RegExp('^' + re + '$', flags)
            } catch (er) /* istanbul ignore next - should be impossible */ {
                // If it was an invalid regular expression, then it can't match
                // anything.  This trick looks for a character after the end of
                // the string, which is of course impossible, except in multi-line
                // mode, but it's not a /m regex.
                return new RegExp('$.')
            }
        
            regExp._glob = pattern
            regExp._src = re
        
            return regExp
        }
        
        minimatch.makeRe = function (pattern, options) {
            return new Minimatch(pattern, options || {}).makeRe()
        }
        
        Minimatch.prototype.makeRe = makeRe
        function makeRe () {
            if (this.regexp || this.regexp === false) return this.regexp
        
            // at this point, this.set is a 2d array of partial
            // pattern strings, or "**".
            //
            // It's better to use .match().  This function shouldn't
            // be used, really, but it's pretty convenient sometimes,
            // when you just want to work with a regex.
            var set = this.set
        
            if (!set.length) {
                this.regexp = false
                return this.regexp
            }
            var options = this.options
        
            var twoStar = options.noglobstar ? star
            : options.dot ? twoStarDot
            : twoStarNoDot
            var flags = options.nocase ? 'i' : ''
        
            var re = set.map(function (pattern) {
                return pattern.map(function (p) {
                    return (p === GLOBSTAR) ? twoStar
                    : (typeof p === 'string') ? regExpEscape(p)
                    : p._src
                }).join('\\\/')
            }).join('|')
        
            // must match entire pattern
            // ending in a * or ** will make it less strict.
            re = '^(?:' + re + ')$'
        
            // can match anything, as long as it's not this.
            if (this.negate) re = '^(?!' + re + ').*$'
        
            try {
                this.regexp = new RegExp(re, flags)
            } catch (ex) /* istanbul ignore next - should be impossible */ {
                this.regexp = false
            }

            return this.regexp
        }
        
        minimatch.match = function (list, pattern, options) {
            options = options || {}
            var mm = new Minimatch(pattern, options)
            list = list.filter(function (f) {
                return mm.match(f)
            })
            if (mm.options.nonull && !list.length) {
                list.push(pattern)
            }
            return list
        }
        
        Minimatch.prototype.match = function match (f, partial) {
            if (typeof partial === 'undefined') partial = this.partial
            this.debug('match', f, this.pattern)
            // short-circuit in the case of busted things.
            // comments, etc.
            if (this.comment) return false
            if (this.empty) return f === ''
        
            if (f === '/' && partial) return true
        
            var options = this.options
        
            // windows: need to use /, not \
            if (path.sep !== '/') {
                f = f.split(path.sep).join('/')
            }
        
            // treat the test path as a set of pathparts.
            f = f.split(slashSplit)
            this.debug(this.pattern, 'split', f)
        
            // just ONE of the pattern sets in this.set needs to match
            // in order for it to be valid.  If negating, then just one
            // match means that we have failed.
            // Either way, return on the first hit.
        
            var set = this.set
            this.debug(this.pattern, 'set', set)
        
            // Find the basename of the path by looking for the last non-empty segment
            var filename
            var i
            for (i = f.length - 1; i >= 0; i--) {
                filename = f[i]
                if (filename) break
            }
        
            for (i = 0; i < set.length; i++) {
                var pattern = set[i]
                var file = f
                if (options.matchBase && pattern.length === 1) {
                    file = [filename]
                }
                var hit = this.matchOne(file, pattern, partial)
                if (hit) {
                    if (options.flipNegate) return true
                    return !this.negate
                }
            }
        
            // didn't get any hits.  this is success if it's a negative
            // pattern, failure otherwise.
            if (options.flipNegate) return false
            return this.negate
        }
        
        // set partial to true to test if, for example,
        // "/a/b" matches the start of "/*/b/*/d"
        // Partial means, if you run out of file before you run
        // out of pattern, then that's fine, as long as all
        // the parts match.
        Minimatch.prototype.matchOne = function (file, pattern, partial) {
            var options = this.options
        
            this.debug('matchOne', { 'this': this, file: file, pattern: pattern })
        
            this.debug('matchOne', file.length, pattern.length)
        
            for (var fi = 0, pi = 0, fl = file.length, pl = pattern.length; (fi < fl) && (pi < pl); fi++, pi++) {
                this.debug('matchOne loop')
                var p = pattern[pi]
                var f = file[fi]
            
                this.debug(pattern, p, f)
            
                // should be impossible.
                // some invalid regexp stuff in the set.
                /* istanbul ignore if */
                if (p === false) return false
            
                if (p === GLOBSTAR) {
                    this.debug('GLOBSTAR', [pattern, p, f])
            
                    // "**"
                    // a/**/b/**/c would match the following:
                    // a/b/x/y/z/c
                    // a/x/y/z/b/c
                    // a/b/x/b/x/c
                    // a/b/c
                    // To do this, take the rest of the pattern after
                    // the **, and see if it would match the file remainder.
                    // If so, return success.
                    // If not, the ** "swallows" a segment, and try again.
                    // This is recursively awful.
                    //
                    // a/**/b/**/c matching a/b/x/y/z/c
                    // - a matches a
                    // - doublestar
                    //   - matchOne(b/x/y/z/c, b/**/c)
                    //     - b matches b
                    //     - doublestar
                    //       - matchOne(x/y/z/c, c) -> no
                    //       - matchOne(y/z/c, c) -> no
                    //       - matchOne(z/c, c) -> no
                    //       - matchOne(c, c) yes, hit
                    var fr = fi
                    var pr = pi + 1
                    if (pr === pl) {
                    this.debug('** at the end')
                    // a ** at the end will just swallow the rest.
                    // We have found a match.
                    // however, it will not swallow /.x, unless
                    // options.dot is set.
                    // . and .. are *never* matched by **, for explosively
                    // exponential reasons.
                    for (; fi < fl; fi++) {
                        if (file[fi] === '.' || file[fi] === '..' ||
                        (!options.dot && file[fi].charAt(0) === '.')) return false
                    }
                    return true
                    }
            
                    // ok, let's see if we can swallow whatever we can.
                    while (fr < fl) {
                    var swallowee = file[fr]
            
                    this.debug('\nglobstar while', file, fr, pattern, pr, swallowee)
            
                    // XXX remove this slice.  Just pass the start index.
                    if (this.matchOne(file.slice(fr), pattern.slice(pr), partial)) {
                        this.debug('globstar found match!', fr, fl, swallowee)
                        // found a match.
                        return true
                    } else {
                        // can't swallow "." or ".." ever.
                        // can only swallow ".foo" when explicitly asked.
                        if (swallowee === '.' || swallowee === '..' ||
                        (!options.dot && swallowee.charAt(0) === '.')) {
                        this.debug('dot detected!', file, fr, pattern, pr)
                        break
                        }
            
                        // ** swallows a segment, and continue.
                        this.debug('globstar swallow a segment, and continue')
                        fr++
                    }
                    }
            
                    // no match was found.
                    // However, in partial mode, we can't say this is necessarily over.
                    // If there's more *pattern* left, then
                    /* istanbul ignore if */
                    if (partial) {
                    // ran out of file
                    this.debug('\n>>> no match, partial?', file, fr, pattern, pr)
                    if (fr === fl) return true
                    }
                    return false
                }
            
                // something other than **
                // non-magic patterns just have to match exactly
                // patterns with magic have been turned into regexps.
                var hit
                if (typeof p === 'string') {
                    hit = f === p
                    this.debug('string match', p, f, hit)
                } else {
                    hit = f.match(p)
                    this.debug('pattern match', p, f, hit)
                }
            
                if (!hit) return false
            }
        
            // Note: ending in / means that we'll get a final ""
            // at the end of the pattern.  This can only match a
            // corresponding "" at the end of the file.
            // If the file ends in /, then it can only match a
            // a pattern that ends in /, unless the pattern just
            // doesn't have any more for it. But, a/b/ should *not*
            // match "a/b/*", even though "" matches against the
            // [^/]*? pattern, except in partial mode, where it might
            // simply not be reached yet.
            // However, a/b/ should still satisfy a/*
        
            // now either we fell off the end of the pattern, or we're done.
            if (fi === fl && pi === pl) {
                // ran out of pattern and filename at the same time.
                // an exact hit!
                return true
            } else if (fi === fl) {
                // ran out of file, but still had pattern left.
                // this is ok if we're doing the match as part of
                // a glob fs traversal.
                return partial
            } else /* istanbul ignore else */ if (pi === pl) {
                // ran out of pattern, still have file left.
                // this is only acceptable if we're on the very last
                // empty segment of a file with a trailing slash.
                // a/* should match a/b/
                return (fi === fl - 1) && (file[fi] === '')
            }
        
            // should be unreachable.
            /* istanbul ignore next */
            throw new Error('wtf?')
        }
        
        // replace stuff like \* with *
        function globUnescape (s) {
            return s.replace(/\\(.)/g, '$1')
        }
        
        function regExpEscape (s) {
            return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&')
        }
        
        return minimatch;
    }

    function onDiagnostic(diagnostic) {
        const msg = ts.flattenDiagnosticMessageText(diagnostic.messageText, '\n');
        console.error(`Error ${diagnostic.code}: ${msg}`);
    }

    class CustomCompilerHost {
        constructor() {
            this.jsDocParsingMode = ts.JSDocParsingMode.ParseNone;
            this.m_doLog = false;
            this.minimatch = requireMinimatch();
        }

        fileExists(fileName) {
            if (this.m_doLog) console.log('fileExists', fileName);
            return fs.existsSync(fileName);
        }

        readFile(fileName) {
            try {
                if (this.m_doLog) console.log('readFile', fileName);
                return fs.readFileTextSync(fileName);
            } catch (e) {
                return undefined;
            }
        }

        directoryExists(directoryName) {
            try {
                if (this.m_doLog) console.log('directoryExists', directoryName);
                
                // Explicitly prevent TypeScript from looking in node_modules/@types
                if (directoryName.includes('node_modules/@types')) {
                    return false;
                }
                
                const stat = fs.statSync(directoryName);
                return stat.type === fs.FileType.Directory;
            } catch (e) {
                return false;
            }
        }

        realpath(filePath) {
            if (this.m_doLog) console.log('realpath', filePath);
            return fs.realPath(filePath);
        }

        getCurrentDirectory() {
            return process.cwd();
        }

        getDirectories(filePath) {
            if (this.m_doLog) console.log('getDirectories', filePath);
            const entries = fs.readDirSync(filePath);
            let dirs = [];

            for (const entry of entries) {
                if (entry.status.type !== fs.FileType.Directory) continue;
                dirs.push(entry.name);
            }

            return dirs;
        }

        useCaseSensitiveFileNames() {
            return true;
        }

        getSourceFile(
            fileName,
            languageVersionOrOptions,
            onError,
            shouldCreateNewSourceFile
        ) {
            try {
                if (this.m_doLog) console.log('getSourceFile', fileName);
                const code = fs.readFileTextSync(fileName);

                return ts.createSourceFile(fileName, code, languageVersionOrOptions);
            } catch (e) {
                console.error('Error getting source file', String(e));
                if (onError) onError(String(e));
                return undefined;
            }
        }

        getDefaultLibFileName(options) {
            return './internal/lib/core.d.ts';
        }

        writeFile(fileName, data, writeByteOrderMark) {
            // TODO: Implement writeByteOrderMark
            fileName = path.normalize(fileName);
            fs.mkdirSync(path.dirname(fileName), true);
            // console.log('writeFile', fileName, path.dirname(fileName));
            fs.writeFileTextSync(fileName, data);
            if (fileName.includes('.js')) {
                try {
                    eval(data);
                } catch (e) {
                    console.error('Error registering compiled module', String(e.stack));
                }
            }
        }

        getCanonicalFileName(fileName) {
            // console.log('getCanonicalFileName', fileName);
            try {
                if (fs.existsSync(fileName)) {
                    return fs.realPath(fileName);
                }
            } catch (e) {}

            const result = path.normalize(fileName);
            return result;
        }

        getNewLine() {
            return '\n';
        }

        getEnvironmentVariable(name) {
            // console.log('getEnvironmentVariable', name);
            return process.env[name];
        }

        readDirectory(
            dirPath,       // string
            extensions,    // string[]
            exclude,       // string[] | undefined
            include,       // string[] | undefined
            depth          // number | undefined
        ) {
            // console.log('readDirectory', dirPath, 'extensions:', extensions, 'exclude:', exclude, 'include:', include, 'depth:', depth);

            try {
                // Get all entries in the directory
                const entries = fs.readDirSync(dirPath || './');
                let allFiles = [];

                const pathPrefix = dirPath.endsWith('/') || dirPath.length === 0 ? dirPath : `${dirPath}/`;

                // Convert entries to full paths
                for (const entry of entries) {
                    if (entry.status.type !== fs.FileType.Regular) continue;
                    allFiles.push(`${pathPrefix}${entry.name}`);
                }

                // console.log('allFiles: [', allFiles.join(', '), ']');

                // Filter by extensions
                let filteredFiles = allFiles;
                if (extensions && extensions.length > 0) {
                    filteredFiles = allFiles.filter(file => {
                        // Check if the file ends with any of the extensions
                        return extensions.some(ext => file.endsWith(ext));
                    });
                }
                
                // Apply include patterns if specified
                if (include && include.length > 0) {
                    filteredFiles = filteredFiles.filter(file => {
                        // Use minimatch for more accurate glob pattern matching
                        return include.some(pattern => {
                            return this.minimatch(file, pattern, { matchBase: true });
                        });
                    });
                }
                
                // Apply exclude patterns if specified
                if (exclude && exclude.length > 0) {
                    filteredFiles = filteredFiles.filter(file => {
                        // Use minimatch for more accurate glob pattern matching
                        return !exclude.some(pattern => {
                            return this.minimatch(file, pattern, { matchBase: true });
                        });
                    });
                }
                
                // Handle recursive search if depth is specified
                if (depth !== undefined) {
                    depth--;

                    if (depth === 0) return filteredFiles;
                }

                // Get subdirectories
                const subdirs = entries.filter(entry => 
                    entry.status.type === fs.FileType.Directory
                ).map(entry => `${pathPrefix}${entry.name}`);
                
                // Recursively search subdirectories
                for (const subdir of subdirs) {
                    const subdirFiles = this.readDirectory(
                        subdir,
                        extensions,
                        exclude,
                        include,
                        depth
                    );
                    filteredFiles = filteredFiles.concat(subdirFiles);
                }
                
                // console.log('readDirectory result: [', filteredFiles.join(', '), ']');
                return filteredFiles;
            } catch (e) {
                console.error('Error in readDirectory:', e);
                return [];
            }
        }
    }

    function compileFile(filePath) {
        try {
            const host = new CustomCompilerHost();
            const program = ts.createProgram(
                [filePath],
                {
                    noEmitOnError: true,
                    noImplicitAny: true,
                    target: ts.ScriptTarget.ES2017,
                    module: ts.ModuleKind.AMD,
                    outDir: './internal/dist',
                    typeRoots: ['./internal/lib'],
                    types: [],
                    skipLibCheck: true,
                    skipDefaultLibCheck: true
                },
                host
            );

            const emitResult = program.emit();
            emitResult.diagnostics.forEach(onDiagnostic);
            
            return !emitResult.emitSkipped;
        } catch (e) {
            console.warn(String(e.stack));
            return false;
        }
    }

    function compileDirectory(dirPath) {
        try {
            const configPath = ts.findConfigFile(
                dirPath,
                fs.existsSync,
                'tsconfig.json'
            );
            if (!configPath) throw new Error(`No tsconfig.json found at path: ${dirPath}`);

            const { config, error } = ts.readConfigFile(configPath, fs.readFileTextSync);
            if (error) throw new Error(`Failed to read tsconfig.json at path: ${configPath}, ${JSON.stringify(error)}`);

            const host = new CustomCompilerHost();

            const { options, fileNames, errors } = ts.parseJsonConfigFileContent(config, host, dirPath);
            if (errors.length > 0) throw new Error(`Failed to parse tsconfig.json at path: ${configPath}, ${JSON.stringify(errors)}`);

            if (!options.module) options.module = ts.ModuleKind.AMD;
            else if (options.module !== ts.ModuleKind.AMD) {
                throw new Error('Only AMD module type is supported');
            }

            if (!options.target) options.target = ts.ScriptTarget.ES2017;
            else if (options.target !== ts.ScriptTarget.ES2017) {
                throw new Error('Only ES2017 target is supported');
            }

            const program = ts.createProgram(fileNames, options, host);
            const emitResult = program.emit();
            emitResult.diagnostics.forEach(onDiagnostic);
            
            return !emitResult.emitSkipped;
        } catch (e) {
            console.warn(String(e.stack));
            return false;
        }
    }

    return {
        compileFile,
        compileDirectory
    }
}