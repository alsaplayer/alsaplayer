/****************************************************************************
** Form implementation generated from reading ui file 'MainWindowBase.ui'
**
** Created: Wed Aug 29 16:54:39 2001
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#include "MainWindowBase.h"

#include <qframe.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qtoolbutton.h>
#include <qlayout.h>
#include <qvariant.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

static const char* const image0_data[] = { 
"19 19 117 2",
"Qt c None",
"#Y c #000000",
".# c #000400",
".n c #001410",
"#m c #001818",
".W c #002000",
".6 c #002018",
".P c #002020",
"#E c #002408",
".Q c #002420",
"#U c #002800",
".0 c #002829",
".o c #002c20",
".w c #002c29",
"#M c #003029",
"#l c #003031",
".x c #003c10",
".2 c #003c20",
".8 c #003c39",
".X c #004039",
"#n c #004041",
"#W c #004441",
"#w c #00484a",
"#. c #004c4a",
".R c #005031",
".1 c #005052",
"#k c #005552",
"#X c #006162",
"#a c #006562",
"## c #00696a",
"#V c #008d8b",
"#o c #009194",
".Z c #00999c",
".E c #00a1a4",
".O c #00a5a4",
".Y c #00aeac",
"#j c #00b2b4",
".9 c #00b6b4",
".N c #00bebd",
".m c #082420",
"#P c #086141",
"#p c #086152",
".c c #102c29",
".i c #103031",
".7 c #103410",
".z c #103818",
"#b c #103820",
"#L c #182c20",
".I c #183c20",
".d c #184441",
".p c #184841",
".y c #184c20",
".M c #18cacd",
".J c #20484a",
".j c #204c4a",
"#x c #20a1a4",
"#v c #293829",
"#T c #295529",
".L c #29cecd",
"#i c #313831",
"#g c #313839",
"#f c #314041",
".a c #314c4a",
".C c #31d2d5",
"#Q c #415d5a",
".F c #416141",
".v c #41bebd",
".D c #41d6d5",
".b c #4a6562",
".V c #525052",
".S c #527152",
"#y c #528162",
".K c #52cecd",
".u c #52dade",
".B c #5adede",
"#D c #627162",
"#S c #627562",
"#F c #627573",
"#R c #627962",
".A c #62cecd",
".s c #62e2e6",
"#h c #6a756a",
"#q c #6a856a",
".t c #6ae2e6",
".5 c #737173",
".H c #737973",
"#u c #7b797b",
".G c #7b7d7b",
"#H c #7b817b",
".l c #7beaee",
"#C c #838183",
"#N c #838583",
"#O c #8bbebd",
"#I c #949994",
".h c #94d6d5",
".r c #94f2f6",
".U c #9c8d8b",
"#t c #9c9194",
".3 c #9ca59c",
".q c #9ce2e6",
"#B c #a49194",
"#c c #a4aaa4",
".e c #aceaee",
".k c #aceeee",
"#K c #bdb2ac",
"#J c #bdbabd",
"#d c #bdbebd",
"#A c #bdc2bd",
".f c #bdffff",
"#e c #c5b2ac",
"#z c #c5b6ac",
".4 c #c5bebd",
"#s c #c5d6bd",
".g c #c5ffff",
".T c #d5bebd",
"#G c #d5dad5",
"#r c #decebd",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt",
"QtQtQtQt.#.a.b.b.b.b.b.cQtQtQtQtQtQtQt",
"QtQtQtQtQt.d.e.f.f.g.g.h.iQtQtQtQtQtQt",
"QtQtQtQtQtQt.j.k.f.f.f.l.h.mQtQtQtQtQt",
"QtQtQtQtQt.n.o.p.q.r.s.t.u.v.wQtQtQtQt",
"QtQtQtQt.n.x.y.z.p.A.B.t.C.D.E.wQtQtQt",
"QtQtQt.n.x.F.G.H.I.J.K.L.M.N.N.O.PQtQt",
"QtQt.Q.R.S.T.U.G.V.W.X.Y.N.N.N.N.Z.nQt",
"Qt.0.1.2.3.4.T.5.0.6.7.8.Y.N.N.9#..0Qt",
"Qt###a#b#c#d.4#e#f#g#h#i.8#j#j#k#l#mQt",
"Qt#n#o#p#q.4#r#s#d#t.G#u#v.X#w#l.nQtQt",
"QtQt.8#x#y#z#s#A.4.T#B#C#D#E.w.nQtQtQt",
"QtQtQt#F#G#H#I#J#d#d#K.H#L#M#mQtQtQtQt",
"QtQtQtQt#N#O#P#Q#R#S#T#U#l.nQtQtQtQtQt",
"QtQtQtQtQt.8#o#a.8.8.Q#M.nQtQtQtQtQtQt",
"QtQtQtQtQtQt#n#V#a#l.w#mQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQt#W#o#X.nQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQt.##YQtQtQtQtQtQtQtQtQt",
"QtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQtQt"};

static const char* const image1_data[] = { 
"15 15 8 1",
". c None",
"# c #000000",
"d c #5a595a",
"a c #949194",
"e c #acaaac",
"c c #acaeac",
"f c #bdbebd",
"b c #ffffff",
".####......###.",
".#ab#.....#cc#.",
".#de#....#abb#.",
".#de#...#acff#.",
".#de#..#aabee#.",
".#de#.#aacfee#.",
".#de##aaabeee#.",
".#de#aeeeeeee#.",
".#de##dddaeee#.",
".#de#.#dddeee#.",
".#de#..#ddaee#.",
".#de#...#ddee#.",
".#de#....#daa#.",
".##a#.....#dd#.",
".####......###."};

static const char* const image2_data[] = { 
"18 18 10 1",
". c None",
"a c #000000",
"h c #5a595a",
"g c #737173",
"d c #949194",
"# c #a4a1a4",
"b c #acaaac",
"e c #acaeac",
"f c #bdbebd",
"c c #ffffff",
"..................",
".#................",
".aa#..............",
".abaa.............",
".acdbaa...........",
".aedddbaa.........",
".abcddddbaa.......",
".abeddddddbaa.....",
".abbcdddddddbaa...",
".abbbfffffffffbaa.",
".abbbggggggghaa...",
".abbgggggghaa.....",
".abbgggghaa.......",
".abggghaa.........",
".abghaa...........",
".ahaa.............",
".aa...............",
".................."};

static const char* const image3_data[] = { 
"15 15 7 1",
". c None",
"# c #000000",
"d c #5a595a",
"c c #949194",
"a c #acaaac",
"e c #bdbebd",
"b c #ffffff",
".###......####.",
".#aa#.....#bc#.",
".#bbc#....#ad#.",
".#eeac#...#ad#.",
".#aabcc#..#ad#.",
".#aaeacc#.#ad#.",
".#aaabccc##ad#.",
".#aaaaaaac#ad#.",
".#aaacddd##ad#.",
".#aaaddd#.#ad#.",
".#aacdd#..#ad#.",
".#aadd#...#ad#.",
".#ccd#....#ad#.",
".#dd#.....#c##.",
".###......####."};

static const char* const image4_data[] = { 
"15 15 6 1",
". c None",
"# c #000000",
"c c #737173",
"d c #acaaac",
"b c #bdbebd",
"a c #ffffff",
"...............",
".#############.",
".#abbbbbbbbbc#.",
".#babbbbbbbcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bbdddddddcc#.",
".#bcccccccc#c#.",
".#cccccccccc##.",
".#############.",
"..............."};

static const char* const image5_data[] = { 
"18 18 12 1",
". c None",
"# c #000000",
"b c #9c009c",
"j c #a4a1a4",
"c c #cd99ff",
"a c #cdcaff",
"h c #e68500",
"e c #ff3000",
"f c #ff9900",
"d c #ff999c",
"i c #ffca00",
"g c #ffca62",
"..................",
"........#.........",
".......#ab#.......",
"......#acacb#.....",
".....#aaacaccb#...",
"....#aaacacccccb#.",
"...#aaaaacaccccc#.",
"..#baaaacaccccc#..",
"..###baaacaccc#...",
"..##de#bcaccc#f#..",
"..#g#dde#bac#fe##.",
"...hg#ddfe##e##...",
"...#gg#ddfe#hii#..",
"....hgg#f#hiiih##.",
"....#ggi#iih##....",
".....hggh##j......",
".....###..........",
".................."};

static const char* const image6_data[] = { 
"14 12 3 1",
". c None",
"# c #000000",
"a c #949194",
"..............",
"..............",
"..####..####..",
"..#aa#..#aa#..",
"..#aa#..#aa#..",
"..#aa#..#aa#..",
"..#aa#..#aa#..",
"..#aa#..#aa#..",
"..#aa#..#aa#..",
"..####..####..",
"..............",
".............."};

static const char* const image7_data[] = { 
"14 12 3 1",
". c None",
"# c #000000",
"a c #949194",
"..............",
"..............",
"........##....",
"......##a#....",
"....##aaa#....",
"..##aaaaa#....",
"..##aaaaa#....",
"....##aaa#....",
"......##a#....",
"........##....",
"..............",
".............."};

static const char* const image8_data[] = { 
"14 12 3 1",
". c None",
"# c #000000",
"a c #949194",
"..............",
"..............",
"...##.........",
"...#a##.......",
"...#aaa##.....",
"...#aaaaa##...",
"...#aaaaa##...",
"...#aaa##.....",
"...#a##.......",
"...##.........",
"..............",
".............."};

static const char* const image9_data[] = { 
"26 9 3 1",
". c None",
"# c #838183",
"a c #eeeaee",
"...#..................#...",
"..#......#########.....#..",
"..#..#...#aaaaaaa#..#..#..",
".#..#.....#aaaaa#....#..#.",
".#..#.#....#aaa#...#.#..#.",
".#..#.....#######....#..#.",
"..#..#....#aaaaa#...#..#..",
"..#.......#######......#..",
"...#..................#..."};

static const char* const image10_data[] = { 
"16 11 3 1",
". c None",
"# c #838183",
"a c #eeeaee",
"................",
"......##....#...",
".###.#a#.....#..",
".#a##aa#..#..#..",
".#a#aaa#...#..#.",
".#a#aaa#.#.#..#.",
".#a#aaa#...#..#.",
".#a##aa#..#..#..",
".###.#a#.....#..",
"......##....#...",
"................"};


/* 
 *  Constructs a MainWindowBase which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 */
MainWindowBase::MainWindowBase( QWidget* parent,  const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    QPixmap image0( ( const char** ) image0_data );
    QPixmap image1( ( const char** ) image1_data );
    QPixmap image2( ( const char** ) image2_data );
    QPixmap image3( ( const char** ) image3_data );
    QPixmap image4( ( const char** ) image4_data );
    QPixmap image5( ( const char** ) image5_data );
    QPixmap image6( ( const char** ) image6_data );
    QPixmap image7( ( const char** ) image7_data );
    QPixmap image8( ( const char** ) image8_data );
    QPixmap image9( ( const char** ) image9_data );
    QPixmap image10( ( const char** ) image10_data );
    if ( !name )
	setName( "MainWindowBase" );
    resize( 368, 121 ); 
    setCaption( tr( "AlsaPlayer" ) );
    setAcceptDrops( TRUE );
    MainWindowBaseLayout = new QVBoxLayout( this ); 
    MainWindowBaseLayout->setSpacing( 4 );
    MainWindowBaseLayout->setMargin( 4 );

    Frame3 = new QFrame( this, "Frame3" );
    QPalette pal;
    QColorGroup cg;
    cg.setColor( QColorGroup::Foreground, black );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 10, 95, 137) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, black );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 10, 95, 137) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 10, 95, 137) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    Frame3->setPalette( pal );
    Frame3->setFrameShape( QFrame::NoFrame );
    Frame3->setFrameShadow( QFrame::Plain );
    Frame3Layout = new QGridLayout( Frame3 ); 
    Frame3Layout->setSpacing( 4 );
    Frame3Layout->setMargin( 2 );

    speedLabelLeft = new QLabel( Frame3, "speedLabelLeft" );
    speedLabelLeft->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)0, speedLabelLeft->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    speedLabelLeft->setPalette( pal );
    speedLabelLeft->setMargin( 0 );
    speedLabelLeft->setText( tr( "Speed:" ) );
    speedLabelLeft->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    Frame3Layout->addWidget( speedLabelLeft, 0, 0 );

    timeLabel = new QLabel( Frame3, "timeLabel" );
    timeLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)5, timeLabel->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    timeLabel->setPalette( pal );
    timeLabel->setMargin( 0 );
    timeLabel->setText( tr( "No time data" ) );

    Frame3Layout->addWidget( timeLabel, 1, 3 );

    speedLabel = new QLabel( Frame3, "speedLabel" );
    speedLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)5, speedLabel->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    speedLabel->setPalette( pal );
    speedLabel->setMargin( 0 );
    speedLabel->setText( tr( "100%" ) );

    Frame3Layout->addWidget( speedLabel, 0, 1 );

    titleLabel = new QLabel( Frame3, "titleLabel" );
    titleLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)5, titleLabel->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, white );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, white );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, white );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    titleLabel->setPalette( pal );
    titleLabel->setMargin( 0 );
    titleLabel->setText( tr( "No stream" ) );

    Frame3Layout->addMultiCellWidget( titleLabel, 0, 0, 2, 3 );

    volumeLabel = new QLabel( Frame3, "volumeLabel" );
    volumeLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, volumeLabel->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    volumeLabel->setPalette( pal );
    volumeLabel->setMargin( 0 );
    volumeLabel->setText( tr( "100%" ) );

    Frame3Layout->addWidget( volumeLabel, 1, 1 );

    volumeLabelLeft = new QLabel( Frame3, "volumeLabelLeft" );
    volumeLabelLeft->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)0, volumeLabelLeft->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    volumeLabelLeft->setPalette( pal );
    volumeLabelLeft->setMargin( 0 );
    volumeLabelLeft->setText( tr( "Volume:" ) );
    volumeLabelLeft->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );

    Frame3Layout->addWidget( volumeLabelLeft, 1, 0 );

    infoLabel = new QLabel( Frame3, "infoLabel" );
    infoLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)5, infoLabel->sizePolicy().hasHeightForWidth() ) );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, QColor( 241, 241, 241) );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setActive( cg );
    cg.setColor( QColorGroup::Foreground, white );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, black );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setInactive( cg );
    cg.setColor( QColorGroup::Foreground, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Button, QColor( 228, 228, 228) );
    cg.setColor( QColorGroup::Light, white );
    cg.setColor( QColorGroup::Midlight, white );
    cg.setColor( QColorGroup::Dark, QColor( 114, 114, 114) );
    cg.setColor( QColorGroup::Mid, QColor( 152, 152, 152) );
    cg.setColor( QColorGroup::Text, black );
    cg.setColor( QColorGroup::BrightText, white );
    cg.setColor( QColorGroup::ButtonText, QColor( 128, 128, 128) );
    cg.setColor( QColorGroup::Base, white );
    cg.setColor( QColorGroup::Background, black );
    cg.setColor( QColorGroup::Shadow, black );
    cg.setColor( QColorGroup::Highlight, QColor( 8, 93, 139) );
    cg.setColor( QColorGroup::HighlightedText, white );
    pal.setDisabled( cg );
    infoLabel->setPalette( pal );
    infoLabel->setMargin( 0 );
    infoLabel->setText( tr( "..." ) );

    Frame3Layout->addWidget( infoLabel, 1, 2 );
    MainWindowBaseLayout->addWidget( Frame3 );

    positionSlider = new QSlider( this, "positionSlider" );
    positionSlider->setMaxValue( 100 );
    positionSlider->setOrientation( QSlider::Horizontal );
    MainWindowBaseLayout->addWidget( positionSlider );

    Layout10 = new QHBoxLayout; 
    Layout10->setSpacing( 4 );
    Layout10->setMargin( 0 );

    Layout9 = new QVBoxLayout; 
    Layout9->setSpacing( 0 );
    Layout9->setMargin( 0 );
    QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    Layout9->addItem( spacer );

    Layout8 = new QHBoxLayout; 
    Layout8->setSpacing( 2 );
    Layout8->setMargin( 0 );

    menuButton = new QToolButton( this, "menuButton" );
    menuButton->setFocusPolicy( QToolButton::TabFocus );
    menuButton->setText( tr( "" ) );
    menuButton->setPixmap( image0 );
    menuButton->setPopupDelay( 0 );
    menuButton->setAutoRaise( TRUE );
    QToolTip::add(  menuButton, tr( "Menu" ) );
    Layout8->addWidget( menuButton );

    previousButton = new QToolButton( this, "previousButton" );
    previousButton->setFocusPolicy( QToolButton::TabFocus );
    previousButton->setText( tr( "" ) );
    previousButton->setPixmap( image1 );
    previousButton->setAutoRaise( TRUE );
    QToolTip::add(  previousButton, tr( "Skip to previous track" ) );
    Layout8->addWidget( previousButton );

    playButton = new QToolButton( this, "playButton" );
    playButton->setFocusPolicy( QToolButton::TabFocus );
    playButton->setText( tr( "" ) );
    playButton->setPixmap( image2 );
    playButton->setAutoRaise( TRUE );
    QToolTip::add(  playButton, tr( "Play" ) );
    Layout8->addWidget( playButton );

    nextButton = new QToolButton( this, "nextButton" );
    nextButton->setFocusPolicy( QToolButton::TabFocus );
    nextButton->setText( tr( "" ) );
    nextButton->setPixmap( image3 );
    nextButton->setAutoRaise( TRUE );
    QToolTip::add(  nextButton, tr( "Skip to next track" ) );
    Layout8->addWidget( nextButton );

    stopButton = new QToolButton( this, "stopButton" );
    stopButton->setFocusPolicy( QToolButton::TabFocus );
    stopButton->setText( tr( "" ) );
    stopButton->setPixmap( image4 );
    stopButton->setAutoRaise( TRUE );
    QToolTip::add(  stopButton, tr( "Stop" ) );
    Layout8->addWidget( stopButton );

    playlistButton = new QToolButton( this, "playlistButton" );
    playlistButton->setFocusPolicy( QToolButton::TabFocus );
    playlistButton->setText( tr( "" ) );
    playlistButton->setPixmap( image5 );
    playlistButton->setAutoRaise( TRUE );
    QToolTip::add(  playlistButton, tr( "Show playlist" ) );
    Layout8->addWidget( playlistButton );
    Layout9->addLayout( Layout8 );
    QSpacerItem* spacer_2 = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    Layout9->addItem( spacer_2 );
    Layout10->addLayout( Layout9 );

    Layout5 = new QVBoxLayout; 
    Layout5->setSpacing( 2 );
    Layout5->setMargin( 0 );

    Layout3 = new QHBoxLayout; 
    Layout3->setSpacing( 2 );
    Layout3->setMargin( 0 );

    pauseButton = new QToolButton( this, "pauseButton" );
    pauseButton->setFocusPolicy( QToolButton::TabFocus );
    pauseButton->setText( tr( "" ) );
    pauseButton->setPixmap( image6 );
    pauseButton->setAutoRaise( TRUE );
    QToolTip::add(  pauseButton, tr( "Pause" ) );
    Layout3->addWidget( pauseButton );

    backButton = new QToolButton( this, "backButton" );
    backButton->setFocusPolicy( QToolButton::TabFocus );
    backButton->setText( tr( "" ) );
    backButton->setPixmap( image7 );
    backButton->setAutoRaise( TRUE );
    QToolTip::add(  backButton, tr( "Backwards, normal speed" ) );
    Layout3->addWidget( backButton );

    forwardButton = new QToolButton( this, "forwardButton" );
    forwardButton->setFocusPolicy( QToolButton::TabFocus );
    forwardButton->setText( tr( "" ) );
    forwardButton->setPixmap( image8 );
    forwardButton->setAutoRaise( TRUE );
    QToolTip::add(  forwardButton, tr( "Forwards, normal speed" ) );
    Layout3->addWidget( forwardButton );

    speedSlider = new QSlider( this, "speedSlider" );
    speedSlider->setMinValue( -100 );
    speedSlider->setMaxValue( 200 );
    speedSlider->setValue( 66 );
    speedSlider->setOrientation( QSlider::Horizontal );
    QToolTip::add(  speedSlider, tr( "Playback speed and direction" ) );
    Layout3->addWidget( speedSlider );
    Layout5->addLayout( Layout3 );

    Layout4 = new QHBoxLayout; 
    Layout4->setSpacing( 2 );
    Layout4->setMargin( 0 );

    PixmapLabel2 = new QLabel( this, "PixmapLabel2" );
    PixmapLabel2->setPixmap( image9 );
    PixmapLabel2->setScaledContents( FALSE );
    QToolTip::add(  PixmapLabel2, tr( "Balance" ) );
    Layout4->addWidget( PixmapLabel2 );

    balanceSlider = new QSlider( this, "balanceSlider" );
    balanceSlider->setMinValue( -100 );
    balanceSlider->setMaxValue( 100 );
    balanceSlider->setValue( 50 );
    balanceSlider->setOrientation( QSlider::Horizontal );
    QToolTip::add(  balanceSlider, tr( "Balance" ) );
    Layout4->addWidget( balanceSlider );

    PixmapLabel1 = new QLabel( this, "PixmapLabel1" );
    PixmapLabel1->setPixmap( image10 );
    PixmapLabel1->setScaledContents( FALSE );
    QToolTip::add(  PixmapLabel1, tr( "Volume" ) );
    Layout4->addWidget( PixmapLabel1 );

    volumeSlider = new QSlider( this, "volumeSlider" );
    volumeSlider->setMaxValue( 100 );
    volumeSlider->setOrientation( QSlider::Horizontal );
    QToolTip::add(  volumeSlider, tr( "Volume" ) );
    Layout4->addWidget( volumeSlider );
    Layout5->addLayout( Layout4 );
    Layout10->addLayout( Layout5 );
    MainWindowBaseLayout->addLayout( Layout10 );

    // signals and slots connections
    connect( previousButton, SIGNAL( clicked() ), this, SLOT( slotPrevious() ) );
    connect( playButton, SIGNAL( clicked() ), this, SLOT( slotPlay() ) );
    connect( nextButton, SIGNAL( clicked() ), this, SLOT( slotNext() ) );
    connect( stopButton, SIGNAL( clicked() ), this, SLOT( slotStop() ) );
    connect( playlistButton, SIGNAL( clicked() ), this, SLOT( slotPlayList() ) );
    connect( pauseButton, SIGNAL( clicked() ), this, SLOT( slotPause() ) );
    connect( backButton, SIGNAL( clicked() ), this, SLOT( slotBack() ) );
    connect( forwardButton, SIGNAL( clicked() ), this, SLOT( slotForward() ) );
    connect( positionSlider, SIGNAL( sliderReleased() ), this, SLOT( slotPositionSliderReleased() ) );
    connect( positionSlider, SIGNAL( sliderPressed() ), this, SLOT( slotPositionSliderPressed() ) );
    connect( speedSlider, SIGNAL( sliderMoved(int) ), this, SLOT( slotSpeedSliderMoved(int) ) );
    connect( balanceSlider, SIGNAL( sliderMoved(int) ), this, SLOT( slotBalanceSliderMoved(int) ) );
    connect( volumeSlider, SIGNAL( sliderMoved(int) ), this, SLOT( slotVolumeSliderMoved(int) ) );

    // tab order
    setTabOrder( menuButton, previousButton );
    setTabOrder( previousButton, playButton );
    setTabOrder( playButton, nextButton );
    setTabOrder( nextButton, stopButton );
    setTabOrder( stopButton, playlistButton );
    setTabOrder( playlistButton, pauseButton );
    setTabOrder( pauseButton, backButton );
    setTabOrder( backButton, forwardButton );
    setTabOrder( forwardButton, speedSlider );
    setTabOrder( speedSlider, balanceSlider );
    setTabOrder( balanceSlider, volumeSlider );
}

/*  
 *  Destroys the object and frees any allocated resources
 */
MainWindowBase::~MainWindowBase()
{
    // no need to delete child widgets, Qt does it all for us
}

void MainWindowBase::slotCDDA()
{
    qWarning( "MainWindowBase::slotCDDA(): Not implemented yet!" );
}

void MainWindowBase::slotAbout()
{
    qWarning( "MainWindowBase::slotAbout(): Not implemented yet!" );
}

void MainWindowBase::slotBack()
{
    qWarning( "MainWindowBase::slotBack(): Not implemented yet!" );
}

void MainWindowBase::slotBalanceSliderMoved(int)
{
    qWarning( "MainWindowBase::slotBalanceSliderMoved(int): Not implemented yet!" );
}

void MainWindowBase::slotFX()
{
    qWarning( "MainWindowBase::slotFX(): Not implemented yet!" );
}

void MainWindowBase::slotForward()
{
    qWarning( "MainWindowBase::slotForward(): Not implemented yet!" );
}

void MainWindowBase::slotNext()
{
    qWarning( "MainWindowBase::slotNext(): Not implemented yet!" );
}

void MainWindowBase::slotPause()
{
    qWarning( "MainWindowBase::slotPause(): Not implemented yet!" );
}

void MainWindowBase::slotPlay()
{
    qWarning( "MainWindowBase::slotPlay(): Not implemented yet!" );
}

void MainWindowBase::slotPlayList()
{
    qWarning( "MainWindowBase::slotPlayList(): Not implemented yet!" );
}

void MainWindowBase::slotPositionSliderPressed()
{
    qWarning( "MainWindowBase::slotPositionSliderPressed(): Not implemented yet!" );
}

void MainWindowBase::slotPositionSliderReleased()
{
    qWarning( "MainWindowBase::slotPositionSliderReleased(): Not implemented yet!" );
}

void MainWindowBase::slotPrevious()
{
    qWarning( "MainWindowBase::slotPrevious(): Not implemented yet!" );
}

void MainWindowBase::slotScopes()
{
    qWarning( "MainWindowBase::slotScopes(): Not implemented yet!" );
}

void MainWindowBase::slotSpeedSliderMoved(int)
{
    qWarning( "MainWindowBase::slotSpeedSliderMoved(int): Not implemented yet!" );
}

void MainWindowBase::slotStop()
{
    qWarning( "MainWindowBase::slotStop(): Not implemented yet!" );
}

void MainWindowBase::slotVolumeSliderMoved(int)
{
    qWarning( "MainWindowBase::slotVolumeSliderMoved(int): Not implemented yet!" );
}

