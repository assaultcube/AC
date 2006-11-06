#import "Launcher.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>
#define kMaxDisplays	16

NSMutableArray *array;
	
static int numberForKey( CFDictionaryRef desc, CFStringRef key )
{
    CFNumberRef value;
    int num = 0;

    if ( (value = CFDictionaryGetValue(desc, key)) == NULL )
        return 0;
    CFNumberGetValue(value, kCFNumberIntType, &num);
    return num;
}


static void listModes(CGDirectDisplayID dspy)
{
    CFArrayRef modeList;
    CFDictionaryRef mode;
    CFIndex i, cnt;

    modeList = CGDisplayAvailableModes(dspy);
    if ( modeList == NULL )
    {
        printf( "Display is invalid\n" );
        return;
    }
    cnt = CFArrayGetCount(modeList);
    //printf( "Display 0x%x: %d modes:\n", (unsigned int)dspy, (int)cnt );
    for ( i = 0; i < cnt; ++i )
    {
        mode = CFArrayGetValueAtIndex(modeList, i);
        [array addObject:[NSString stringWithFormat:@"%i x %i", numberForKey(mode, kCGDisplayWidth), numberForKey(mode, kCGDisplayHeight)]];

    }
}

@implementation MyObject

- (IBAction)playAction:(id)sender
{
	NSArray *res = [[resolutions titleOfSelectedItem] componentsSeparatedByString:@" x "];	
	NSMutableArray *args = [[NSMutableArray alloc] init];
	NSString *cwd = [[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"actioncube"];
	
	[args addObject:[NSString stringWithFormat:@"-w%@", [res objectAtIndex:0]]];
	[args addObject:[NSString stringWithFormat:@"-h%@", [res objectAtIndex:1]]];
	
	if([fullscreen state] == NSOffState) [args addObject:@"-t"];
	
	
	NSTask *task = [[NSTask alloc] init];
	[task setCurrentDirectoryPath:cwd];
	[task setLaunchPath:[cwd stringByAppendingPathComponent:@"actioncube.app/Contents/MacOS/actioncube"]];
	[task setArguments:args];
	[args release];
	
	BOOL okay = YES;
	
	NS_DURING
		[task launch];
		[NSApp terminate:self];
	NS_HANDLER
		NSRunAlertPanel(@"Error", @"Can't start Sauerbraten! Please move the directory containing Sauerbraten to a path that doesn't contain weird characters or start Sauerbraten manually.", @"OK", NULL, NULL);
		okay = NO;
	NS_ENDHANDLER
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    CGDirectDisplayID display[kMaxDisplays];
    CGDisplayCount numDisplays;
    CGDisplayCount i;
    CGDisplayErr err;

	array = [[NSMutableArray alloc] init];
	
    err = CGGetActiveDisplayList(kMaxDisplays,
                                 display,
                                 &numDisplays);
    if ( err != CGDisplayNoErr )
    {
        printf("Cannot get displays (%d)\n", err);
        exit( 1 );
    }
    
    //printf( "%d displays found\n", (int)numDisplays );
    for ( i = 0; i < numDisplays; ++i )
        listModes(display[i]);
	
	[resolutions removeAllItems];
	
	int h;
	for (h = 0; h < [array count]; h++)
	{
		id haha = [resolutions itemWithTitle:[array objectAtIndex:h]];
		if (haha == nil)
			[resolutions addItemWithTitle:[array objectAtIndex:h]];
	}
	
	[resolutions selectItemAtIndex: [[NSUserDefaults standardUserDefaults] integerForKey:@"resolution"]];
}
@end