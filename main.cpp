//#define USE_CORE_OPENGL

#include "common.h"
#include "shader.h"

#include "FreeImage.h"

#ifndef APIENTRY
   #define APIENTRY
#endif

void TW_CALL toggle_fullscreen_callback( void * )
{
   glutFullScreenToggle();
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

class sample_t
{
public:
   sample_t();
   ~sample_t();

   void init_buffer();
   void init_vertex_array();
   void draw_frame( float time_from_start );
   void load_texture();

private:
   bool wireframe_;
   GLuint vs_, fs_, program_;
   GLuint vx_buf_;
   GLuint vao_;
   quat   rotation_by_control_;


   GLuint tex_;
};

sample_t::sample_t()
   : wireframe_(false)
{
#ifdef USE_CORE_OPENGL
   TwInit(TW_OPENGL_CORE, NULL);
#else
   TwInit(TW_OPENGL, NULL);
#endif
   // Определение "контролов" GUI
   TwBar *bar = TwNewBar("Parameters");
   TwDefine(" Parameters size='500 150' color='70 100 120' valueswidth=220 iconpos=topleft");

   TwAddVarRW(bar, "Wireframe mode", TW_TYPE_BOOLCPP, &wireframe_, " true='ON' false='OFF' key=w");

   TwAddButton(bar, "Fullscreen toggle", toggle_fullscreen_callback, NULL,
               " label='Toggle fullscreen mode' key=f");

   TwAddVarRW(bar, "ObjRotation", TW_TYPE_QUAT4F, &rotation_by_control_,
              " label='Object orientation' opened=true help='Change the object orientation.' ");

   // Создание шейдеров
   vs_ = create_shader(GL_VERTEX_SHADER  , "shaders//0.glslvs");
   fs_ = create_shader(GL_FRAGMENT_SHADER, "shaders//0.glslfs");
   // Создание программы путём линковки шейдерова
   program_ = create_program(vs_, fs_);
   // Создание буфера с вершинными данными
   init_buffer();
   // Создание VAO
   init_vertex_array();

   load_texture();
}

sample_t::~sample_t()
{
   // Удаление русурсов OpenGL
   glDeleteProgram(program_);
   glDeleteShader(vs_);
   glDeleteShader(fs_);
   glDeleteVertexArrays(1, &vao_);
   glDeleteBuffers(1, &vx_buf_);

   TwDeleteAllBars();
   TwTerminate();
}

void sample_t::load_texture()
{
   string const file_name = "lenna.jpg";

   FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
   //pointer to the image, once loaded
   FIBITMAP *dib(0);
   fif = FreeImage_GetFileType(file_name.c_str(), 0);
   if(fif == FIF_UNKNOWN) 
      fif = FreeImage_GetFIFFromFilename(file_name.c_str());
   if(FreeImage_FIFSupportsReading(fif))
      dib = FreeImage_Load(fif, file_name.c_str());
   //if the image failed to load, return failure
   if(!dib)
      return;

	BYTE* bits(0);
	//image width and height
	unsigned int width(0), height(0);

   	bits = FreeImage_GetBits(dib);
	//get the image width and height
	width = FreeImage_GetWidth(dib);
	height = FreeImage_GetHeight(dib);
	//if this somehow one of these failed (they shouldn't), return failure
	if((bits == 0) || (width == 0) || (height == 0))
		return ;

   glGenTextures(1, &tex_);
   glBindTexture(GL_TEXTURE_2D, tex_);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, bits);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D, 0);

   FreeImage_Unload(dib);
}


void sample_t::init_buffer()
{
   // Создание пустого буфера
   glGenBuffers(1, &vx_buf_);
   // Делаем буфер активным
   glBindBuffer(GL_ARRAY_BUFFER, vx_buf_);

   // Данные для визуализации
   vec2 const data[3] =
   {
        vec2(-1, -1)
      , vec2( 1, -1)
      , vec2( 1,  1)
   };

   // Копируем данные для текущего буфера на GPU
   glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * 3, data, GL_STATIC_DRAW);

   // Сбрасываем текущий активный буфер
   glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void sample_t::init_vertex_array()
{
   glGenVertexArrays(1, &vao_);
   glBindVertexArray(vao_);
      // присоединяем буфер vx_buf_ в vao
      glBindBuffer(GL_ARRAY_BUFFER, vx_buf_);

      // запрашиваем индек аттрибута у программы, созданные по входным шейдерам
      GLuint const pos_location = glGetAttribLocation(program_, "in_pos");
      // устанавливаем формам данных для аттрибута "pos_location"
      // 2 float'а ненормализованных, шаг между вершиными равен sizeof(vec2), смещение от начала буфера равно 0
      glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), 0);
      // "включаем" аттрибут "pos_location"
      glEnableVertexAttribArray(pos_location);
   glBindVertexArray(0);
};

void sample_t::draw_frame( float time_from_start )
{
   float const rotation_angle = 0 * time_from_start * 90;

   float const w                = (float)glutGet(GLUT_WINDOW_WIDTH);
   float const h                = (float)glutGet(GLUT_WINDOW_HEIGHT);
   // строим матрицу проекции с aspect ratio (отношением сторон) таким же, как у окна
   mat4  const proj             = perspective(45.0f, w / h, 0.1f, 100.0f);
   // преобразование из СК мира в СК камеры
   mat4  const view             = lookAt(vec3(0, 0, 1), vec3(0, 0, 0), vec3(0, 1, 0));
   // анимация по времени
   quat  const rotation_by_time = quat(vec3(radians(rotation_angle), 0, 0));
   mat4  const modelview        = view * mat4_cast(rotation_by_control_ * rotation_by_time);
   mat4  const mvp              = proj * modelview;

   // выбор режима растеризации - только рёбра или рёбра + грани
   if (wireframe_)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   // выключаем отсечение невидимых поверхностей
   glDisable(GL_CULL_FACE);
   // выключаем тест глубины
   glDisable(GL_DEPTH_TEST);
   // очистка буфера кадра
   glClearColor(0.2f, 0.2f, 0.2f, 1);
   glClearDepth(1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   // установка шейдеров для рисования
   glUseProgram(program_);

   // установка uniform'ов
   GLuint const mvp_location = glGetUniformLocation(program_, "mvp");
   glUniformMatrix4fv(mvp_location, 1, GL_FALSE, &mvp[0][0]);

   GLuint const time_location = glGetUniformLocation(program_, "time");
   glUniform1f(time_location, time_from_start);

   glBindTexture(GL_TEXTURE_2D, tex_);
   GLuint const tex_location = glGetUniformLocation(program_, "tex");
   glUniform1i(tex_location, 0);

   // установка vao (буфер с данными + формат)
   glBindVertexArray(vao_);

   // отрисовка
   glDrawArrays(GL_TRIANGLES, 0, 3);

   glBindTexture(GL_TEXTURE_2D, 0);
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

unique_ptr<sample_t> g_sample;

// отрисовка кадра
void display_func()
{
   static chrono::system_clock::time_point const start = chrono::system_clock::now();

   // вызов функции отрисовки с передачей ей времени от первого вызова
   g_sample->draw_frame(chrono::duration<float>(chrono::system_clock::now() - start).count());

   // отрисовка GUI
   TwDraw();

   // смена front и back buffer'а (напоминаю, что у нас используется режим двойной буферизации)
   glutSwapBuffers();
}

// Переисовка кадра в отсутствии других сообщений
void idle_func()
{
   glutPostRedisplay();
}

void keyboard_func( unsigned char button, int x, int y )
{
   if (TwEventKeyboardGLUT(button, x, y))
      return;

   switch(button)
   {
   case 27:
     // g_sample.reset();
      exit(0);
   }
}

// Отработка изменения размеров окна
void reshape_func( int width, int height )
{
   if (width <= 0 || height <= 0)
      return;
   glViewport(0, 0, width, height);
   TwWindowSize(width, height);
}

// Очищаем все ресурсы, пока контекст ещё не удалён
void close_func()
{
   g_sample.reset();
}

// callback на различные сообщения от OpenGL
void APIENTRY gl_debug_proc(  GLenum         //source
                              , GLenum         type
                              , GLuint         //id
                              , GLenum         //severity
                              , GLsizei        //length
                              , GLchar const * message
                              
                              , GLvoid * //user_param
                                )
{
   if (type == GL_DEBUG_TYPE_ERROR_ARB)
   {
      cerr << message << endl;
      exit(1);
   }
}

int main( int argc, char ** argv )
{
   // Размеры окна по-умолчанию
   size_t const default_width  = 800;
   size_t const default_height = 800;

   glutInit               (&argc, argv);
   glutInitWindowSize     (default_width, default_height);
   // Указание формата буфера экрана:
   // - GLUT_DOUBLE - двойная буферизация
   // - GLUT_RGB - 3-ёх компонентный цвет
   // - GLUT_DEPTH - будет использоваться буфер глубины
   glutInitDisplayMode    (GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   // Создаем контекст версии 3.3
   glutInitContextVersion (3, 3);
   // Контекст будет поддерживать отладку и "устаревшую" функциональность, которой, например, может пользоваться библиотека AntTweakBar
   glutInitContextFlags   (GLUT_FORWARD_COMPATIBLE | GLUT_DEBUG);
   // Указание либо на core либо на compatibility профил
   glutInitContextProfile (GLUT_COMPATIBILITY_PROFILE );
   int window_handle = glutCreateWindow("OpenGL basic sample");

   // Инициализация указателей на функции OpenGL
   if (glewInit() != GLEW_OK)
   {
      cerr << "GLEW init failed" << endl;
      return 1;
   }

   // Проверка созданности контекста той версии, какой мы запрашивали
   if (!GLEW_VERSION_3_3)
   {
      cerr << "OpenGL 3.3 not supported" << endl;
      return 1;
   }

#ifdef USE_CORE_OPENGL
   glutDestroyWindow(window_handle);
   glutInitContextProfile(GLUT_CORE_PROFILE);
   window_handle = glutCreateWindow("OpenGL basic sample");
#endif

   // Трассировка ошибок по callback'у
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
   glDebugMessageCallbackARB(gl_debug_proc, NULL);
   // выключить все трассировки
   glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE           , GL_DONT_CARE, 0, NULL, false);
   // включить сообщения только об ошибках
   glDebugMessageControlARB(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR_ARB, GL_DONT_CARE, 0, NULL, true );

   // подписываемся на оконные события
   glutReshapeFunc(reshape_func);
   glutDisplayFunc(display_func);
   glutIdleFunc   (idle_func   );
   glutCloseFunc  (close_func  );
   glutKeyboardFunc(keyboard_func);

   // подписываемся на события для AntTweakBar'а
   glutMouseFunc        ((GLUTmousebuttonfun)TwEventMouseButtonGLUT);
   glutMotionFunc       ((GLUTmousemotionfun)TwEventMouseMotionGLUT);
   glutPassiveMotionFunc((GLUTmousemotionfun)TwEventMouseMotionGLUT);
   glutSpecialFunc      ((GLUTspecialfun    )TwEventSpecialGLUT    );
   TwGLUTModifiersFunc  (glutGetModifiers);

   try
   {
      // Создание класса-примера
      g_sample.reset(new sample_t());
      // Вход в главный цикл приложения
      glutMainLoop();
   }
   catch( std::exception const & except )
   {
      std::cout << except.what() << endl;
      return 1;
   }

   return 0;
}
