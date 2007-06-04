#import <Cocoa/Cocoa.h>

@class ConsoleView;

@interface Launcher : NSObject {
	IBOutlet NSWindow *window;
	IBOutlet NSView *view1;
	IBOutlet NSView *view2;
	IBOutlet NSView *view3;
	
	IBOutlet NSArrayController *maps;
	IBOutlet NSArrayController *keys;
    IBOutlet NSPopUpButton *resolutions;
	IBOutlet NSButton *multiplayer;
	IBOutlet ConsoleView *console;
@private	
	NSMutableDictionary *toolBarItems;
	pid_t server;
}

- (IBAction)playAction:(id)sender;

- (IBAction)multiplayerAction:(id)sender;

- (IBAction)helpAction:(id)sender;
@end