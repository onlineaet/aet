"AET" has been developed for two years. It is a Object-oriented Programming Language that is 
compatible with C language, has simple syntax, is flexible and extensible, like c++, ada,
objc and so on in GCC platform.

"AET" contains almost all the features of object-oriented programming languages, 
supports generic programming (substitution and virtualization) and syntax stage optimization. 
When using C language, it is very difficult to use object-oriented theory and method for complex programming. With "AET", 
we can enjoy the simplicity and efficiency brought by C language, and at the same time, we can also use
The complex software is constructed by using the programming method of surface object.



“轻言蔓语”历时2年开发，是一门面象对象、兼容c语言、语法简单、灵活可扩展的编程语言，就像gcc平台中的c++、ada、objc等。

“轻言蔓语”包含几乎所有面象对象程序语言的特性，支持泛型编程（替换和虚化）以及语法阶段优化。在使用C语言时，很
难用面向对象理论和方法进行复杂的编程。有了“轻言蔓语”后，我们可以一边享受c语言带来的简单、高效，同时也可采用
面象对象的编程方法来构建复杂的软件。

如果你感觉使用c++太过复杂，“轻言蔓语”是一个很好的选择。

“轻言蔓语”可以编写运行在各种芯片平台上的系统软件和应用软件。

“轻言蔓语”， 表达它不仅灵活而且扩展性强。翻译成英文是：active expandable translator， 
缩写aet,也称A语言。所以大家在代码里会看到很多“aet”单词。

市场上已有了很多开发语言，为什么自已还要再发明一门新语言呢？
作者之前有过多年的java开发经验，最近几年用c开发。发现在写c时，时常感觉很难用面象对象的方法来写软件。
因为c语言里没有对象、类、接口、抽象、继承、重载、重写等面象对象的语言属性，就算实现了某些面象对象的特性，也感觉很别扭，
不自然。另外，现代软件语言的一个重要功能泛型在c里更是泛善可陈。
使用c语言大家有一个诉求就是速度，因为c语言编译器总是在为各种硬件平台提供优化的机器代码。特别是像gcc这样的开源编译器
让我们看到在语法分析和中间代码生成阶段是如何优化用户的代码，使之运行更快。比如优化while循环、优化for循环、优化返回值等等。
所以作者认为构建出自已的语境,可以在将来更好的为提速A语言所写的软件作好准备。

 由于完全包含c语言的功能。这里只列出aet特有的一些特性：
“轻言蔓语”所提供的关键字如下，基本反映了aet所有的功能。
aet是基于gcc开发的，为了区别于c++的关键字，除了"__OBJECT__"关键字外，所有的关键字最后一个字母都是"$"
   "class$"     用于声明一个类。在类中可以声明变量和方法，也可定义类中的静态方法。但不能定义类中的方法。类中方法只能在impl$关键字中实现。
   "abstract$"  表示抽象的意思，是一个修饰符。只可修饰类和修饰类方法；
                 当用abstract$修饰类时，此类就是抽象类，如果修饰方法，方法所在的类必须是抽象类，抽象类不能通过new$生成对象。
   "interface$"  用于定义接口。接口只包含常量和方法。接口方法的实现由用implements$关键字的类提供。
  
   "extends$",   可以声明一个类是从另外一个类继承而来的,接口不能使用extends$继承另外一个接口。
  "implements$"  是一个类实现一个接口用的关键字
   "impl$"       在关键字impl$中才能实现类中声明的方法。类中声明的变量的初始值可在构造方法中赋值,也可定义不在类中声明的其它方法。
   "final$"      可以修饰类(被修饰的类不能被继承）、属性、方法（不能改变量值，子类不能重写。
   "new$"        创建类实例,创建对象过程中会执行类的构造方法。
   "super$"      在子类内部调用父类的方法和变量, super$() 调用父类的构造函数，只能在当前构造函数的第一行。super$->setxxx();调用父类的方法setxxx  
   "package$"    区分两个名字相同的类。在aet实现中，类名分用户类名和系统类名。系统类名由包名+用户类名生成。
   "public$"     public$修饰的类、属性、方法和构造方法可以被任何类访问。  
   "protected$"  protected$修饰的类、属性、方法和构造方法可以被同一个包中的类或子类访问。   
   "private$"    只能由本类访问。   
   "enum$"       定义枚举。
   "varof$"      测试它左边的变量是否是它右边的AET类或接口的实例。  
   "generic_info$"  获取泛型信息,
   "genericblock$"   泛型块函数，只有在泛型块函数内的泛型声明才会被真实类型替换。其它都被虚化成void
   "__OBJECT__"      用以指示本行语句所在的源文件中的类名。
   "aet_goto_compile$"  aet编译器内部使用。
   
   实现引用记数器回收对象的机制（释放内存）。
   单根继承结构,所有类都继承自AObject
   
   除了引入泛型，需要二次编译，其它与c的编译速度基本相同。
   
   
   