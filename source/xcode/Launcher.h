#import <Cocoa/Cocoa.h>

@interface MyObject : NSObject
{
    IBOutlet NSPopUpButton *resolutions;
    IBOutlet NSButtonCell *fullscreen;

}
- (IBAction)playAction:(id)sender;
@end