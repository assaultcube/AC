#import "Launcher.h"
#import "ConsoleView.h"
#include <stdlib.h>
#include <unistd.h> /* _exit() */
#include <util.h> /* forkpty() */

// User default keys
#define dkVERSION @"version"
#define dkFULLSCREEN @"fullscreen"
#define dkFSAA @"fsaa"

#define dkRESOLUTION @"resolution"
#define dkADVANCEDOPTS @"advancedOptions"
#define dkSERVEROPTS @"server_options"
#define dkDESCRIPTION @"server_description"
#define dkPASSWORD @"server_password"
#define dkMAXCLIENTS @"server_maxclients"

#define kMaxDisplays	16

//If you make a MOD then please change this, the bundle indentifier, the file extensions (.cgz, .dmo), and the url registration.
#define kASSAULTCUBE @"assaultcube"
#define kUSERCONFGIDIR @"assaultcube/v1.2next"

//tab names, i.e. image names (text is localised)
#define tkMAIN @"Main"
#define tkKEYS @"Keys"
#define tkSERVER @"Server"


@interface NSString(Extras)
@end
@implementation NSString(Extras)
- (NSString*)expand {
    NSMutableString *str = [NSMutableString string];
    [str setString:self];
    [str replaceOccurrencesOfString:@":s" withString:kASSAULTCUBE options:0 range:NSMakeRange(0, [str length])]; 
    return str;
}
@end


@interface NSUserDefaults(Extras) // unless you want strings with "(null)" in them :-/
- (NSString*)nonNullStringForKey:(NSString*)key;
@end
@implementation NSUserDefaults(Extras)
- (NSString*)nonNullStringForKey:(NSString*)key {
    NSString *result = [self stringForKey:key];
    return (result ? result : @"");
}
@end


@interface Map : NSObject {
    NSString *path;
    BOOL demo, user;
}
@end
@implementation Map
- (id)initWithPath:(NSString*)aPath user:(BOOL)aUser demo:(BOOL)aDemo
{
    if((self = [super init])) 
    {
        path = [[aPath stringByDeletingPathExtension] retain];
        user = aUser;
        demo = aDemo;
    }
    return self;
}
- (void)dealloc 
{
    [path release];
    [super dealloc];
}
- (NSString*)path { return (demo ? [NSString stringWithFormat:@"--loaddemo=%@", path] : path); } // minor hack
- (NSString*)name { return [path lastPathComponent]; }
- (NSImage*)image 
{ 
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:[path stringByAppendingString:@".jpg"]]; 
    if(!image && demo) image = [NSImage imageNamed:tkMAIN];
    if(!image) image = [NSImage imageNamed:@"Nomap"];
    return image;
}
- (NSString*)text 
{
    NSString *text = [NSString alloc];
    NSError *error;
    if([text respondsToSelector:@selector(initWithContentsOfFile:encoding:error:)])
        text = [text initWithContentsOfFile:[path stringByAppendingString:@".txt"] encoding:NSASCIIStringEncoding error:&error];
    else
        text = [text initWithContentsOfFile:[path stringByAppendingString:@".txt"]]; //deprecated in 10.4
    if(!text) text = (demo)?@"Recorded demo data":@"";
    return text;
}
- (void)setText:(NSString*)text { } // wtf? - damn textfield believes it's editable
- (NSString*)tickIfExists:(NSString*)ext 
{
    unichar tickCh = 0x2713; 
    return ([[NSFileManager defaultManager] fileExistsAtPath:[path stringByAppendingString:ext]] ? [NSString stringWithCharacters:&tickCh length:1] : @"");
}
- (NSString*)hasImage { return [self tickIfExists:@".jpg"]; }
- (NSString*)hasText { return [self tickIfExists:@".txt"]; }
- (NSString*)hasCfg { return [self tickIfExists:@".cfg"]; }
- (NSString*)user { 
    unichar tickCh = 0x2713; 
    return (user ? [NSString stringWithCharacters:&tickCh length:1] : @"");
}
@end


static int numberForKey(CFDictionaryRef desc, CFStringRef key) 
{
    CFNumberRef value;
    int num = 0;
    if ((value = CFDictionaryGetValue(desc, key)) == NULL)
        return 0;
    CFNumberGetValue(value, kCFNumberIntType, &num);
    return num;
}


@interface Launcher(ToolBar)
@end
@implementation Launcher(ToolBar)

- (void)switchViews:(NSToolbarItem *)item 
{
    //20/7/13: RR Removing keys submenu
//    NSView *views[] = {view1, view3, view4};
    NSView *views[] = {view1, view4};
    NSView *prefsView = views[[item tag]-1];
    
    //to stop flicker, we make a temp blank view.
    NSView *tempView = [[NSView alloc] initWithFrame:[[window contentView] frame]];
    [window setContentView:tempView];
    [tempView release];
    
    //mojo to get the right frame for the new window.
    NSRect newFrame = [window frame];
    newFrame.size.height = [prefsView frame].size.height + ([window frame].size.height - [[window contentView] frame].size.height);
    newFrame.size.width = [prefsView frame].size.width;
    newFrame.origin.y += ([[window contentView] frame].size.height - [prefsView frame].size.height);
    
    //set the frame to newFrame and animate it. 
    [window setFrame:newFrame display:YES animate:YES];
    //set the main content view to the new view we have picked through.
    [window setContentView:prefsView];
    [window setContentMinSize:[prefsView bounds].size];
}

- (void)initToolBar 
{
    toolBarItems = [[NSMutableDictionary alloc] init];
    NSEnumerator *e = [[self toolbarDefaultItemIdentifiers:nil] objectEnumerator];
    NSString *identifier;
    while(identifier = [e nextObject])
    {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
        int tag = [identifier intValue];
        NSString *name = identifier;
        SEL action = @selector(displayHelp:);
        id target = self;
        if(tag) {
            //20/7/13: RR Removing keys submenu
//            NSString *names[] = {tkMAIN, tkKEYS, tkSERVER};
            NSString *names[] = {tkMAIN, tkSERVER};
            name = names[tag-1];
            action = @selector(switchViews:);
            target = self;
        }
        [item setTag:tag];
        [item setTarget:target];
        [item setAction:action];
        [item setLabel:NSLocalizedString(name, @"")];
        [item setImage:[NSImage imageNamed:name]];
        [toolBarItems setObject:item forKey:identifier];
        [item release];
    }
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@""];
    [toolbar setDelegate:self]; 
    [toolbar setAllowsUserCustomization:NO]; 
    [toolbar setAutosavesConfiguration:NO];  
    [window setToolbar:toolbar]; 
    [toolbar release];
    if([window respondsToSelector:@selector(setShowsToolbarButton:)]) [window setShowsToolbarButton:NO]; //10.4+
    
    //select the first by default
    NSToolbarItem *first = [toolBarItems objectForKey:[[self toolbarDefaultItemIdentifiers:nil] objectAtIndex:0]];
    [toolbar setSelectedItemIdentifier:[first itemIdentifier]];
    [self switchViews:first]; 
}

#pragma mark toolbar delegate methods

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag 
{
    return [toolBarItems objectForKey:itemIdentifier];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)theToolbar 
{
    return [self toolbarDefaultItemIdentifiers:theToolbar];
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar 
{
    NSMutableArray *array = (NSMutableArray *)[self toolbarSelectableItemIdentifiers:toolbar];
    [array addObject:NSToolbarFlexibleSpaceItemIdentifier];
    [array addObject:@"Help"];
    return array;
}

- (NSArray *)toolbarSelectableItemIdentifiers: (NSToolbar *)toolbar 
{
    NSMutableArray *array = [NSMutableArray array];
    //20/7/13: RR Removing keys submenu
//    NSView *views[] = {view1, view3, view4};
    NSView *views[] = {view1, view4};
    int i;
    for(i = 0; i < sizeof(views)/sizeof(NSView*); i++) if(views[i]) [array addObject:[NSString stringWithFormat:@"%d", i+1]];
    return array;
}

- (void)displayHelp:(id)sender 
{
	NSString *path = [[[NSBundle mainBundle] resourcePath] stringByAppendingString:@"/help/README.html"];

	if (![[NSWorkspace sharedWorkspace] openFile:path])
		NSLog(@"Warning: [[NSWorkspace sharedWorkspace] openFile:path] failed");
	
}
@end


@implementation Launcher

/* directory where the executable lives */
+ (NSString *)cwd
{
    return [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/gamedata"];
}


/* directory where user files are kept - typically /Users/<name>/Application Support/assaultcube_major.minor */
+ (NSString*)userdir 
{
    FSRef folder;
    NSString *path = nil;
    if(FSFindFolder(kUserDomain, kApplicationSupportFolderType, NO, &folder) == noErr) {
        CFURLRef url = CFURLCreateFromFSRef(kCFAllocatorDefault, &folder);
        path = [(NSURL *)url path];
        CFRelease(url);
        path = [path stringByAppendingPathComponent:kUSERCONFGIDIR];
        NSFileManager *fm = [NSFileManager defaultManager];
        if(![fm fileExistsAtPath:path]) [fm createDirectoryAtPath:path attributes:nil]; //ensure it exists    
    }
    return path;
}

- (void)addResolutionsForDisplay:(CGDirectDisplayID)dspy 
{
    CFIndex i, cnt;
    CFArrayRef modeList = CGDisplayAvailableModes(dspy);
    if(modeList == NULL) return;
    cnt = CFArrayGetCount(modeList);
    for(i = 0; i < cnt; i++) {
        CFDictionaryRef mode = CFArrayGetValueAtIndex(modeList, i);
        NSString *title = [NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)];
        if(![resolutions itemWithTitle:title]) [resolutions addItemWithTitle:title];
    }	
}

- (void)initResolutions 
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    [resolutions removeAllItems];
    if(CGGetActiveDisplayList(kMaxDisplays, display, &numDisplays) == CGDisplayNoErr) 
    {
        CGDisplayCount i;
        for (i = 0; i < numDisplays; i++)
            [self addResolutionsForDisplay:display[i]];
    }
    [resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:dkRESOLUTION]];	
}

/* build key array from config data */
-(NSArray *)getKeys:(NSDictionary *)dict 
{	
    NSMutableArray *arr = [NSMutableArray array];
    NSEnumerator *e = [dict keyEnumerator];
    NSString *key;
    while ((key = [e nextObject])) 
    {
        int pos = [key rangeOfString:@"bind."].location;
        if(pos == NSNotFound || pos > 5) continue;
        [arr addObject:[NSDictionary dictionaryWithObjectsAndKeys: //keys used in nib
            [key substringFromIndex:pos+5], @"key",
            [key substringToIndex:pos], @"mode",
            [dict objectForKey:key], @"action",
            nil]];
    }
    return arr;
}

/*
 * extract a dictionary from the config files containing:
 * - name, team, gamma strings
 * - bind/editbind '.' key strings
 */
-(NSDictionary *)readConfigFiles 
{
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    [dict setObject:@"" forKey:@"name"]; //ensure these entries are never nil
    [dict setObject:@"" forKey:@"team"]; 
    
    NSMutableArray *lines = [NSMutableArray array];
    NSString *files[] = {@"config/saved.cfg", @"config/autoexec.cfg"};

    int i;
    for(i = 0; i < sizeof(files)/sizeof(NSString*); i++) 
    {
        NSString *file = [Launcher userdir];
        file = [file stringByAppendingPathComponent:files[i]];
        
        [lines addObjectsFromArray:[[NSString stringWithContentsOfFile:file usedEncoding:NULL error:NULL] componentsSeparatedByString:@"\n"]]; // 10.4+
        
        if(i==0 && ![lines count])  // ugh - special case when first run...
        {
            NSString *defaultfiles[] = {@"config/defaults.cfg", @"config/resetbinds.cfg"};
            
            int j;
            for(j = 0; j < sizeof(defaultfiles)/sizeof(NSString*); j++)
            {
                file = [[Launcher cwd] stringByAppendingPathComponent:defaultfiles[j]];
                [lines addObjectsFromArray:[[NSString stringWithContentsOfFile:file usedEncoding:NULL error:NULL] componentsSeparatedByString:@"\n"]]; // 10.4+
            }
        }
		
        NSString *line; 
        NSEnumerator *e = [lines objectEnumerator];
        while(line = [e nextObject]) 
        {
            NSRange r; // more flexible to do this manually rather than via NSScanner...
            int j = 0;
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            if(j != 0) continue; // shouldn't be indented
            r.location = j;
            while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
            r.length = j - r.location;
            NSString *type = [line substringWithRange:r];
			
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            if(j < [line length] && [line characterAtIndex:j] == '"') 
            {
                r.location = ++j;
                while(j < [line length] && [line characterAtIndex:j] != '"') j++; //until close quote
                r.length = (j++) - r.location;
            } else {
                r.location = j;
                while(j < [line length] && [line characterAtIndex:j] > ' ') j++; //until white
                r.length = j - r.location;
            }
            if(r.location+r.length >= [line length]) continue; //missing value
            NSString *value = [line substringWithRange:r];
            
            while(j < [line length] && [line characterAtIndex:j] <= ' ') j++; //skip white
            NSString *remainder = [line substringFromIndex:j];
			
            if([type isEqual:@"name"] || [type isEqual:@"team"] || [type isEqual:@"gamma"]) 
                [dict setObject:value forKey:type];
            else if([type isEqual:@"bind"] || [type isEqual:@"editbind"] || [type isEqual:@"specbind"]) 
                [dict setObject:remainder forKey:[NSString stringWithFormat:@"%@.%@", type,value]];
        }
    }
    return dict;
}

- (void)killServer {
    if(server > 0) kill(server, SIGKILL); //@WARNING - you do NOT want a 0 or -1 to be accidentally sent a  kill!
    server = -1;
    [multiplayer setTitle:NSLocalizedString(@"Start", @"")];
    [console appendText:@"\n \n"];
}

- (void)serverDataAvailable:(NSNotification *)note
{
    NSFileHandle *taskOutput = [note object];
    NSData *data = [[note userInfo] objectForKey:NSFileHandleNotificationDataItem];
	
    if (data && [data length])
    {
        NSString *text = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
        [console appendText:text];
        [text release];					
        [taskOutput readInBackgroundAndNotify]; //wait for more data
    }
    else
    {
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc removeObserver:self name:NSFileHandleReadCompletionNotification object:taskOutput];
        close([taskOutput fileDescriptor]);
        [self killServer];
    }
}

- (BOOL)launchGame:(NSArray *)args {
    NSString *cwd = [Launcher cwd];
    NSString *exe = [[NSBundle bundleWithPath:[cwd stringByAppendingPathComponent:[@":s.app" expand]]] executablePath];
    
    BOOL okay = YES;
    
    if([args containsObject:@"-d"])
    {
        if(server != -1) return NO; // server is already running
        
        const char **argv = (const char**)malloc(sizeof(char*)*([args count] + 2)); //{path, <args>, NULL};
        argv[0] = [exe fileSystemRepresentation];        
        argv[[args count]+1] = NULL;
        int i;
        for(i = 0; i < [args count]; i++) argv[i+1] = [[args objectAtIndex:i] UTF8String];  
        
        int fdm;
        NSString *fail = [NSLocalizedString(@"ServerAlertMesg", nil) expand];
        switch ( (server = forkpty(&fdm, NULL, NULL, NULL)) ) // forkpty so we can reliably grab SDL console
        { 
            case -1:
                [console appendLine:fail];
                [self killServer];
                okay = NO;
                break;
            case 0: // child
                chdir([cwd fileSystemRepresentation]);
                if(execv([exe fileSystemRepresentation], (char*const*)argv) == -1) fprintf(stderr, "%s\n", [fail UTF8String]);
                _exit(0);
            default: // parent
                [multiplayer setTitle:NSLocalizedString(@"Stop", @"")];
                NSFileHandle *taskOutput = [[NSFileHandle alloc] initWithFileDescriptor:fdm];
                NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
                [nc addObserver:self selector:@selector(serverDataAvailable:) name:NSFileHandleReadCompletionNotification object:taskOutput];
                [taskOutput readInBackgroundAndNotify];
                break;
        }
        free(argv);
    } 
    else 
    {
        NS_DURING
            NSTask *task = [[NSTask alloc] init];
            [task setCurrentDirectoryPath:cwd];
			[task setLaunchPath:exe];
			[task setArguments:args]; NSLog(@"%@",[args description]);
            [task setEnvironment:[NSDictionary dictionaryWithObjectsAndKeys: 
                @"1", @"SDL_SINGLEDISPLAY",
                @"1", @"SDL_ENABLEAPPEVENTS", nil
            ]]; // makes Command-H, Command-M and Command-Q work at least when not in fullscreen

            [task launch];
            if(server == -1) [NSApp terminate:self]; //if there is a server then don't exit!
        NS_HANDLER
            //NSLog(@"%@", localException);
            NSBeginCriticalAlertSheet(
                [NSLocalizedString(@"ClientAlertTitle", @"") expand] , nil, nil, nil,
                window, nil, nil, nil, nil,
                @"%@", [NSLocalizedString(@"ClientAlertMesg", @"") expand]);
            okay = NO;
        NS_ENDHANDLER
    }

    return okay;
}

/*
 * nil will just launch the fps game
 * "-e.." will launch and run commands
 * otherwise we are specifying a map to play
 */
- (BOOL)playFile:(id)filename 
{	
    NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
    
    NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];	
    NSMutableArray *args = [NSMutableArray array];

    [args addObject:[NSString stringWithFormat:@"--home=%@", [Launcher userdir]]];
    [args addObject:@"--init"];

    [args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
    [args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
    [args addObject:@"-z32"]; //otherwise seems to have a fondness to use -z16 which looks crap
    [args addObject:[NSString stringWithFormat:@"-a%d", [defs integerForKey:dkFSAA]]];
    [args addObject:[NSString stringWithFormat:@"-t%d", [defs integerForKey:dkFULLSCREEN]]];

    if ([stencil state] == NSOnState)
        [args addObject:@"-s8"];

    NSMutableArray *cmds = [NSMutableArray array];

    if(filename) 
    {
        if([filename hasPrefix:@"-e"])
            [cmds addObject:[filename substringFromIndex:2]];
        if([filename hasPrefix:[NSString stringWithFormat:@"%@://", kASSAULTCUBE]])
            [args addObject:filename];
        else
        {
            if([filename hasPrefix:@"--loaddemo"])
                [args addObject:filename];
            else
                [args addObject:[NSString stringWithFormat:@"--loadmap=%@", filename]];
        }
    }
    
    if([cmds count] > 0) 
    {
        NSString *script = [cmds objectAtIndex:0];
        int i;
        for(i = 1; i < [cmds count]; i++) script = [NSString stringWithFormat:@"%@;%@", script, [cmds objectAtIndex:i]];
        [args addObject:[NSString stringWithFormat:@"-e%@", script]];
    }

    NSEnumerator *e = [[[defs nonNullStringForKey:dkADVANCEDOPTS] componentsSeparatedByString:@" "] objectEnumerator];
    NSString *opt;
    while(opt = [e nextObject]) if([opt length] != 0) [args addObject:opt]; //skip empty ones 

    return [self launchGame:args];
}

- (void)scanMaps:(id)obj //@note threaded!
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int i;
    for(i = 0; i < 2; i++) 
    {
        NSString *dir = (i==0) ? [Launcher cwd] : [Launcher userdir];
        NSDirectoryEnumerator *enumerator = [[NSFileManager defaultManager] enumeratorAtPath:dir];
        NSString *file;
        while(file = [enumerator nextObject]) 
        {
            NSString *role = [fileRoles objectForKey:[file pathExtension]];
            if(role) 
            {   
                Map *map = [[Map alloc] initWithPath:[dir stringByAppendingPathComponent:file] user:(i==1) demo:[role isEqual:@"Viewer"]];
                [maps performSelectorOnMainThread:@selector(addObject:) withObject:map waitUntilDone:NO];
            }
        }
    }
    [prog performSelectorOnMainThread:@selector(stopAnimation:) withObject:nil waitUntilDone:NO];
    [pool release];
}

- (void)initMaps 
{
    [prog startAnimation:nil];
    [maps removeObjects:[maps arrangedObjects]];
    [NSThread detachNewThreadSelector: @selector(scanMaps:) toTarget:self withObject:nil];
}

- (void)awakeFromNib 
{
    //generate some pretty icons if they are missing
    NSRect region = NSMakeRect(0, 0, 64, 64);
    NSImage *image = [NSImage imageNamed:tkMAIN];
    if(!image) {
        image = [[NSImage imageNamed:@"NSApplicationIcon"] copy];
        [image setSize:region.size];
        [image setName:tkMAIN]; //one less image to include
    }
    
    [self initToolBar];
    [window setBackgroundColor:[NSColor colorWithDeviceRed:0.90 green:0.90 blue:0.90 alpha:1.0]]; //Apples 'mercury' crayon color

    //from the plist we determine that dmo->Viewer, and cgz->Editor 
    fileRoles = [[NSMutableDictionary dictionary] retain];
    NSEnumerator *types = [[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDocumentTypes"] objectEnumerator];
    NSDictionary *type;
    while((type = [types nextObject])) {
        NSString *role = [type objectForKey:@"CFBundleTypeRole"];
        NSEnumerator *exts = [[type objectForKey:@"CFBundleTypeExtensions"] objectEnumerator];
        NSString *ext;
        while((ext = [exts nextObject])) [fileRoles setObject:role forKey:ext];
    }
	
    NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];    
    NSFileManager *fm = [NSFileManager defaultManager];
    
    NSString *appVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
    NSString *version = [defs stringForKey:dkVERSION];
    if(!version || ![version isEqual:appVersion]) 
    {
        NSLog(@"Upgraded Version...");
        //need to flush lurking config files - they're automatically generated, so no big deal...
        NSString *dir = [Launcher userdir];
        [fm removeFileAtPath:[dir stringByAppendingPathComponent:@"init.cfg"] handler:nil];
        [fm removeFileAtPath:[dir stringByAppendingPathComponent:@"saved.cfg"] handler:nil];
    }
    [defs setObject:appVersion forKey:dkVERSION];
    
    NSDictionary *dict = [self readConfigFiles];
    [keys addObjects:[self getKeys:dict]];
    	
    [self initMaps];
    [self initResolutions];
    server = -1;
    [NSApp setDelegate:self]; //so can catch the double-click, dropped files, termination
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(getUrl:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];    
}


#pragma mark -
#pragma mark application delegate

- (void)applicationDidFinishLaunching:(NSNotification *)note {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *dir = [Launcher cwd];
    if(![fm fileExistsAtPath:dir])
        NSBeginCriticalAlertSheet(
            [NSLocalizedString(@"InitAlertTitle", @"") expand], nil, nil, nil,
            window, self, nil, nil, nil,
            @"%@", [NSLocalizedString(@"InitAlertMesg", @"") expand]);
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (void)applicationWillTerminate: (NSNotification *)note {
    [self killServer];
}

//we register 'cgz' and 'dmo' as doc types
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename 
{
    NSString *role = [fileRoles objectForKey:[filename pathExtension]];
    if(!role) return NO;
    BOOL demo = [role isEqual:@"Viewer"];
    filename = [filename stringByDeletingPathExtension]; //chop off extension
    int i;
    for(i = 0; i < 2; i++) {
        NSString *pkg = (i == 0) ? [Launcher cwd] : [Launcher userdir];
        if(!demo) pkg = [pkg stringByAppendingPathComponent:@"packages/maps/"];            
        if([filename hasPrefix:pkg])
        {
            filename = [filename lastPathComponent]; //chop off extension
            return [self playFile:(demo ? [NSString stringWithFormat:@"--loaddemo=%@", filename] : filename)];
        }
    }
    NSBeginCriticalAlertSheet(
        [NSLocalizedString(@"FileAlertTitle", @"") expand], NSLocalizedString(@"Ok", @""), NSLocalizedString(@"Cancel", @""), nil,
        window, self, @selector(openPackageFolder:returnCode:contextInfo:), nil, nil,
        @"%@", [NSLocalizedString(@"FileAlertMesg", @"") expand]);
    return NO;
}

- (void)openPackageFolder:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo 
{
    if(returnCode == 0) return;
    [self openUserdir:nil]; 
}

//we register 'assaultcube' as a url scheme
- (void)getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSURL *url = [NSURL URLWithString:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]];
    if(!url) return;
    [self playFile:[[event paramDescriptorForKeyword:keyDirectObject] stringValue]]; //use the game internal parser
    //[self playFile:[NSString stringWithFormat:@"-econnect %@", [url host]]]; 
}

#pragma mark interface actions

- (IBAction)multiplayerAction:(id)sender 
{ 
    [window makeFirstResponder:window]; //ensure fields are exited and committed
    if(server != -1) 
    {
        [self killServer]; 
    }
    else 
    {
        NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
    
        NSMutableArray *args = [NSMutableArray arrayWithObject:@"-d"];

        NSEnumerator *e = [[[defs nonNullStringForKey:dkSERVEROPTS] componentsSeparatedByString:@" "] objectEnumerator];
        NSString *opt;
        while(opt = [e nextObject]) if([opt length] != 0) [args addObject:opt]; //skip empty ones
        
        NSString *desc = [defs nonNullStringForKey:dkDESCRIPTION];
        if (![desc isEqual:@""]) [args addObject:[NSString stringWithFormat:@"-n%@", desc]];
        
        NSString *pass = [defs nonNullStringForKey:dkPASSWORD];
        if (![pass isEqual:@""]) [args addObject:[NSString stringWithFormat:@"-p%@", pass]];

		if (![[admin_password stringValue] isEqual:@""]) [args addObject:[NSString stringWithFormat:@"-x%@", pass]];

        int clients = [defs integerForKey:dkMAXCLIENTS];
        if (clients > 0) [args addObject:[NSString stringWithFormat:@"-c%d", clients]];

        //server doesn't support --home 
        //[args addObject:[NSString stringWithFormat:@"--home%@", [Launcher userdir]]];
        
        [self launchGame:args];
    } 
}

- (IBAction)playAction:(id)sender 
{ 
    [window makeFirstResponder:window]; //ensure fields are exited and committed
    [self playFile:nil]; 
}

- (IBAction)playMap:(id)sender
{
    NSArray *sel = [maps selectedObjects];
    if(sel && [sel count] > 0) [self playFile:[[sel objectAtIndex:0] path]];
}

- (IBAction)openUserdir:(id)sender 
{
    [[NSWorkspace sharedWorkspace] openFile:[Launcher userdir]];
}

@end
