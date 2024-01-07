#include "libxml/parser.h"
#include "libxml/tree.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>

// 定义幻数
#define LEDS_MAGIC 'l'
// 定义命令的最大序数
#define LEDS_MAX_NR 4

// 定义LED的魔幻数
#define LED1 _IO(LEDS_MAGIC, 0)
#define LED2 _IO(LEDS_MAGIC, 1)
#define LED3 _IO(LEDS_MAGIC, 2)
#define LED4 _IO(LEDS_MAGIC, 3)
// 灯的亮灭状态
#define LED_ON 0
#define LED_OFF 1

#define DEVCIE_LEDS "/dev/leds_misc"
#define DEVICE_BUZZ "/dev/buzz_misc"
#define BUZZ_MAGIC 'b'
#define BUZZ_ON _IOW(BUZZ_MAGIC, 1, unsigned long)
#define BUZZ_OFF _IOW(BUZZ_MAGIC, 0, unsigned long)

int tcp_socket = 0;

int parsing_xml_from_memory(const char *xml_str, xmlChar **id) {
  printf("Parsing XML from memory start!\n");
  assert(xml_str);

  xmlDocPtr doc;                             // xml整个文档的树形结构
  xmlNodePtr cur, child_cur, grandchild_cur; // xml节点
  xmlChar *value;

  // 获取树形结构
  if ((doc = xmlReadMemory(xml_str, strlen(xml_str), "noname.xml", NULL, 0)) ==
      NULL) {
    return -1;
  }

  // 获取根节点
  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    fprintf(stderr, "Root is empty.\n");
    xmlFreeDoc(doc);
    xmlCleanupParser(); // 清理解析器状态
    return -1;
  }

  if ((xmlStrcmp(cur->name, (const xmlChar *)"nlp"))) {
    fprintf(stderr, "The root is not nlp.\n");
    xmlFreeDoc(doc);
    xmlCleanupParser(); // 清理解析器状态
    return -1;
  }

  // 遍历处理根节点的每一个子节点
  cur = cur->xmlChildrenNode;

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *)"result"))) {

      child_cur = cur->xmlChildrenNode;
      while (child_cur != NULL) {
        if ((!xmlStrcmp(child_cur->name, (const xmlChar *)"object"))) {

          grandchild_cur = child_cur->xmlChildrenNode;
          while (grandchild_cur != NULL) {
            if ((!xmlStrcmp(grandchild_cur->name, (const xmlChar *)"dial"))) {
              value = xmlGetProp(grandchild_cur, "id");
              *id = xmlStrdup(value);
              xmlFree(value);
              printf("Parsing XML from memory end!\n");
              return 0;
            }
            grandchild_cur = grandchild_cur->next;
          }
        }
        child_cur = child_cur->next;
      }
    }
    cur = cur->next;
  }

  xmlFreeDoc(doc);
  xmlCleanupParser(); // 清理解析器状态

  printf("Parsing XML from memory end!\n");

  return 0;
}

int send_pcm_file(char *filename) {
  struct stat buf;

  stat(filename, &buf);

  long len = 160000;

  char file_buf[4096] = {0};

  int fd = open("cmd.pcm", O_RDWR);

  printf("send pcm file start\n");

  int count = 0;

  while (1) {

    int rt = read(fd, file_buf, 4096);

    if (rt <= 0) {
      break;
    }

    rt = send(tcp_socket, file_buf, rt, 0);
    count += rt;

    usleep(3000);
  }

  printf("send pcm file end\n");

  printf("send file success!\n");

  close(fd);
}

int network_init(char *address, int port) {
  int rt;

  // 1. 创建TCP套接字
  tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

  if (tcp_socket == -1) {
    printf("tcp socket failed\n");
  } else {
    printf("tcp socket success!\n");
  }

  int optval = 1;
  rt = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

  if (rt == -1) {
    printf("setsockopt failed\n");
  } else {
    printf("setsockopt success!\n");
  }

  // 2. 将套接字与IP和端口绑定 （连接服务器的准备）

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(address); // 自动获取本机IP
  addr.sin_port = htons(port);               // 端口号PORT

  int addrlen = sizeof(struct sockaddr_in);
  // 链接服务器
  rt = connect(tcp_socket, (struct sockaddr *)&addr, addrlen);

  if (rt != -1) {
    printf("connect success!\n");
  } else {
    printf("connect failed!\n");
  }

  return 0;
}

int *lcd_ptr;
int lcd_fd, ts_fd;

int lcd_draw_point(int i, int j, int color) {
  *(lcd_ptr + 800 * j + i) = color;
}

int lcd_draw_bmp(const char *pathname, int x, int y, int w, int h) {
  int i, j;
  // a 打开图片文件
  int bmp_fd = open(pathname, O_RDWR);
  // 错误处理
  if (bmp_fd == -1) {
    printf("open bmp file failed!\n");
    return -1;
  }
  // 2，将图片数据加载到lcd屏幕
  char header[54];
  char rgb_buf[w * h * 3];
  // a 将图片颜色数据读取出来
  read(bmp_fd, header, 54);
  int pad = (4 - (w * 3) % 4) % 4;
  for (i = 0; i < h; i++) {
    // 读取图片颜色数据
    read(bmp_fd, &rgb_buf[w * i * 3], w * 3);
    // 跳过无效字节
    lseek(bmp_fd, pad, SEEK_CUR);
  }
  // b 加载数据到lcd屏幕
  //  int r = 0xef, g = 0xab, b = 0xcd;
  //  int color = 0xefabcd;
  // int color = b;
  //  遇1结果则为1
  //  b : 00000000 00000000 00000000 11001101
  //  g : 00000000 00000000 10101011 00000000
  //  color : 00000000 00000000 10101011 11001101
  //  1000 = 800*1+200
  //  1800 = 800*2+200
  // 24 --- 32
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      int b = rgb_buf[(j * w + i) * 3 + 0];
      int g = rgb_buf[(j * w + i) * 3 + 1];
      int r = rgb_buf[(j * w + i) * 3 + 2];
      int color = b;
      color |= (g << 8);
      color |= (r << 16);
      // lcd_ptr[800*j+i] = color;
      //*(lcd_ptr+800*j+i) = color;
      lcd_draw_point(i + x, h - 1 - j + y, color);
    }
  }
  // 3，关闭文件
  // a 关闭图片文件
  close(bmp_fd);
  return 0;
}

int dev_init() {
  // 1,打开设备文件
  lcd_fd = open("/dev/fb0", O_RDWR);
  // 错误处理
  if (lcd_fd == -1) {
    printf("open lcd device failed!\n");
    return -1;
  }
  // 2,为lcd设备建立内存映射关系
  lcd_ptr =
      mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
  if (lcd_ptr == MAP_FAILED) {
    printf("mmap failed!\n");
    return -1;
  }
  ts_fd = open("/dev/input/event0", O_RDWR);
  // 错误处理
  if (ts_fd == -1) {
    printf("open ts device failed!\n");
    return -1;
  }
  return 0;
}

int dev_uninit() {
  munmap(lcd_ptr, 800 * 480 * 4);
  // b 关闭设备文件
  close(lcd_fd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage : %s <address> <port>\n", argv[0]);
    return -1;
  }

  int buzz_fd, leds_fd;
  // 打开设备
  buzz_fd = open(DEVICE_BUZZ, O_RDWR); // 打开设备，成功返回0
  if (buzz_fd == -1) {
    fprintf(stderr, "Can not open %s : %s !\n", DEVICE_BUZZ, strerror(errno));
    return -1;
  }
  leds_fd = open(DEVCIE_LEDS, O_RDWR); // 打开设备下的LED，成功返回0
  if (leds_fd == -1) {
    fprintf(stderr, "Can not open %s : %s !\n", DEVCIE_LEDS, strerror(errno));
    return 0;
  }

  network_init(argv[1], atoi(argv[2]));

  int img_idx = 0;
  char img_name[10];
  char buf[4096];
  while (1) {
    // 执行程序外的命令 发送之前先录制
    printf("hit any key to continue: ");
    getchar();
    system("arecord -d5 -c1 -r16000 -traw -fS16_LE cmd.pcm");

    sleep(5);

    send_pcm_file("cmd.pcm");

    memset(buf, '\0', sizeof(buf));
    recv(tcp_socket, buf, 4096, 0);
    printf("%s\n", buf);

    xmlChar *id = NULL;
    parsing_xml_from_memory(buf, &id);

    printf("DEBUG:\n%s\n", id);
    int id_num = atoi((char *)id);

    printf("%d\n", id_num);
    switch (id_num) {
    case 1:
      ioctl(leds_fd, LED1, LED_ON);
      printf("DEBUG: led on\n");
      break;

    case 2:
      ioctl(leds_fd, LED1, LED_OFF);
      printf("DEBUG: led off\n");
      break;

    case 3:
      ioctl(buzz_fd, BUZZ_ON);
      printf("DEBUG: buzz on\n");
      break;

    case 4:
      ioctl(buzz_fd, BUZZ_OFF);
      printf("DEBUG: buzz off\n");
      break;

    case 5:
      img_idx = 1;
      dev_init();
      lcd_draw_bmp("1.bmp", 0, 0, 800, 480);
      break;

    // uninit img
    case 6:
      memset(lcd_ptr, 0, 800 * 480 * 4);
      break;

    case 7:
      ++img_idx;
      if (img_idx > 6) {
        printf("idx out of range!\n");
        img_idx = 6;
      }
      memset(img_name, '\0', sizeof(img_name));
      sprintf(img_name, "%d.bmp", img_idx);
      lcd_draw_bmp(img_name, 0, 0, 800, 480);
      break;

    case 8:
      --img_idx;
      if (img_idx < 1) {
        printf("idx out of range!\n");
        img_idx = 1;
      }
      memset(img_name, '\0', sizeof(img_name));
      sprintf(img_name, "%d.bmp", img_idx);
      lcd_draw_bmp(img_name, 0, 0, 800, 480);
      break;

    case 9:
      system("mplayer 1.mp4 -slave -quiet -zoom -x 800 -y 480 -geometry 0:0 "
             "-vo fbdev2 -vf scale -ac mad -cache 8192 &");
      break;

    case 10:
      system("killall -9 mplayer");
      memset(lcd_ptr, 0, 800 * 480 * 4);
      break;

    case 11:
      printf("DEBUG: play music\n");
      system("aplay here.wav");
      break;

    default:
      printf("command id not found!\n");
    }
  }

  return 0;
}
